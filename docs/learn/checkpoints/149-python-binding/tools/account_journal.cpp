#include "meta/journal_store.h"
#include "meta/journal_http.h"
#include "meta/account_bootstrap.h"
#include <charconv>
#include <cstring>
#include <cstdlib>
#include <iostream>
namespace {
std::string fault;
bool writer(const std::string& path,const std::string& body) {
    const auto name=std::filesystem::u8path(path).filename().string();
    const bool idle=body.find("\"state\":\"idle\"")!=std::string::npos;
    if ((fault=="fail-account" && name=="account.json") ||
        (fault=="fail-clear" && name=="change.pending.json" && idle)) return false;
    const bool ok=study_files::write_private_file(path,body);
    if (ok && ((fault=="crash-pending" && name=="change.pending.json" && !idle) ||
               (fault=="crash-recovery" && name=="recovery.json") ||
               (fault=="crash-account" && name=="account.json"))) std::_Exit(70);
    if (ok && ((fault=="uncertain-pending" && name=="change.pending.json" && !idle) ||
               (fault=="uncertain-account" && name=="account.json") ||
               (fault=="uncertain-clear" && name=="change.pending.json" && idle))) return false;
    return ok;
}
int report(study_meta::JournalResult result) {
    using S=study_meta::JournalState;
    switch(result.state) {
    case S::complete:std::cout<<"complete id="<<result.player_id<<"\n";return 0;
    case S::idle:std::cout<<"idle\n";return 0;
    case S::blocked:std::cout<<"local files blocked; preserve them\n";return 4;
    case S::pending:std::cout<<"outcome uncertain; retry same journal\n";return 8;
    case S::rejected:std::cout<<"request rejected; use current recovery file\n";return 9;
    case S::local_incomplete:std::cout<<"server accepted; local save incomplete\n";return 10;
    }
    return 1;
}
}
int main(int argc,char** argv) {
    if(argc<4 || argc>5)return 2;
    int port=0;const char* end=argv[1]+std::strlen(argv[1]);
    const auto parsed=std::from_chars(argv[1],end,port);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || port<1 || port>65535)return 2;
    const std::string action=argv[3];fault=argc==5?argv[4]:"";
    if (!fault.empty() && fault!="fail-account" && fault!="fail-clear" &&
        fault!="crash-pending" && fault!="crash-recovery" && fault!="crash-account" &&
        fault!="uncertain-pending" && fault!="uncertain-account" && fault!="uncertain-clear")return 2;
    try {
        study_meta::JournalHttp api(port);
        {
            study_meta::JournalStore store(std::filesystem::u8path(argv[2]),api.origin(),writer);
            if(action!="resume" && action!="connect") {
                const auto p=store.prepare(action);
                if(!p || !store.begin(*p))return report({study_meta::JournalState::blocked,0});
            }
            const auto result=study_meta::resume_change(store,api);
            if(action!="connect" || result.state!=study_meta::JournalState::idle)return report(result);
        } // release the shared profile lock before entering normal bootstrap
        study_meta::AccountHttp account_api(port);
        study_meta::LocalAccountStore store(std::filesystem::u8path(argv[2]),account_api.origin());
        study_meta::AccountBootstrap bootstrap;
        const auto result=bootstrap.connect(store,account_api);
        if(result.state==study_meta::BootstrapState::online) {
            std::cout<<"online id="<<result.profile->player_id<<"\n";return 0;
        }
        std::cout<<"account bootstrap incomplete; preserve files\n";return 4;
    } catch(const std::exception&) {std::cerr<<"account operation unavailable; preserve files\n";return 1;}
}
