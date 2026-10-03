#include "meta/account_bootstrap.h"
#include <charconv>
#include <cstring>
#include <iostream>
// Console fixture: one worker, one profile folder, no secret output.
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    int port=0;const char* end=argv[1]+std::strlen(argv[1]);
    const auto parsed=std::from_chars(argv[1],end,port);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || port<1 || port>65535)return 2;
    try {
        study_meta::AccountHttp api(port);
        study_meta::LocalAccountStore store(std::filesystem::u8path(argv[2]),api.origin());
        study_meta::AccountBootstrap session;
        for (;;) {
            const auto result=session.connect(store,api);
            using S=study_meta::BootstrapState;
            switch(result.state) {
            case S::online:
                std::cout<<"online id="<<result.profile->player_id<<" bp="<<result.profile->bp<<"\n";return 0;
            case S::offline_saved:std::cout<<"saved key; verification unavailable\n";return 3;
            case S::storage_blocked:std::cout<<"profile unavailable, damaged, busy or for another server\n";return 4;
            case S::key_rejected:std::cout<<"saved key rejected; keep files for recovery\n";return 5;
            case S::creation_unconfirmed:std::cout<<"issuance unconfirmed; automatic creation stopped\n";return 6;
            case S::unsaved:
                std::cout<<"key not confirmed saved; keep this process open. Enter retry or quit\n";
                std::string command;
                if(!std::getline(std::cin,command) || command!="retry")return 7;
                break;
            }
        }
    } catch(const std::exception&) {std::cerr<<"account bootstrap unavailable\n";return 1;}
}
