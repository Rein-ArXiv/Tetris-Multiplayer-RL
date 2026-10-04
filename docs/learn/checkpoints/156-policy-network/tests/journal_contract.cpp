#include "meta/journal_store.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace study_meta;
void check(bool value) { if(!value)throw std::runtime_error("journal contract"); }
PendingChange sample() { return {"http://127.0.0.1:9999","rotate",std::string(32,'a'),std::string(32,'b'),"rc1."+std::string(64,'c'),42}; }
struct Store {
    std::vector<std::string> calls;int fail=0;bool raises=false;bool available()const{return true;}
    std::string origin()const{return sample().origin;}
    LoadedChange load_pending(){return {JournalRead::ready,sample()};}
    bool step(std::string name,int n){calls.push_back(name);if(fail==n&&raises)throw std::runtime_error("fault");return fail!=n;}
    bool save_recovery(const PendingChange&,std::uint64_t){return step("recovery",1);}
    bool save_access(const PendingChange&,std::uint64_t){return step("access",2);}
    bool remove_pending(){return step("clear",3);}
};
struct Api {
    RemoteChange result{RemoteState::accepted,42,7}; bool raises=false;int calls=0;
    bool valid()const{return true;} std::string origin()const{return sample().origin;}
    RemoteChange change(const PendingChange& p){++calls;check(pending_json(p)==pending_json(sample()));if(raises)throw std::runtime_error("network");return result;}
};
int main(){try{
    check(parse_pending(pending_json(sample()),sample().origin).has_value());
    auto bad=sample();bad.operation="backup";check(!valid_pending(bad));
    bad=sample();bad.next_token=bad.credential;check(!valid_pending(bad));
    bad=sample();bad.operation="recover";bad.credential=bad.next_recovery;check(!valid_pending(bad));
    check(!parse_pending(pending_json(sample()),"http://another.invalid"));
    for(int fail=0;fail<=3;++fail)for(bool raises:{false,true}){
        Store store;store.fail=fail;store.raises=raises;Api api;
        const auto r=resume_change(store,api);
        check(r.state==(fail?JournalState::local_incomplete:JournalState::complete));
        check(r.player_id==(fail?0:42));check(api.calls==1);
        const std::vector<std::string> all{"recovery","access","clear"};
        check(store.calls==std::vector<std::string>(all.begin(),all.begin()+(fail?fail:3)));
    }
    for(auto remote:{RemoteState::uncertain,RemoteState::rejected}){
        Store store;Api api;api.result.state=remote;
        auto r=resume_change(store,api);check(r.state==(remote==RemoteState::rejected?JournalState::rejected:JournalState::pending));
        check(store.calls.size()==(remote==RemoteState::rejected?1u:0u));
    }
    {Store store;Api api;api.result.player_id=43;check(resume_change(store,api).state==JournalState::pending);check(store.calls.empty());}
    {Store store;Api api;api.raises=true;check(resume_change(store,api).state==JournalState::pending);check(store.calls.empty());}
    {Store store;store.fail=3;Api api;api.result.state=RemoteState::rejected;check(resume_change(store,api).state==JournalState::pending);}
    std::cout<<"strict pending, exact request, save order, rejected/uncertain, ID mismatch and exception states passed\n";
    return 0;
}catch(const std::exception&){std::cerr<<"journal contract failed\n";return 1;}}
