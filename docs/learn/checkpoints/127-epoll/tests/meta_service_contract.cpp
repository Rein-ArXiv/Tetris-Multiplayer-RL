#include "meta/memory_results.h"
#include "meta/wire.h"
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace study_meta;
void check(bool ok) {if(!ok)throw std::runtime_error("meta service contract");}
int main(){
    try {
        study_net::MatchRecord record{17,1,101,202,11,100,0,0,0,study_net::MatchRecord::b};
        MemoryResults<2> store;
        check(store.count()==0 && !store.lookup(17));
        const auto first=store.put(record);check(first.status==PutStatus::accepted && first.receipt.row==1);
        auto copy=*store.lookup(17);++copy.score_a;check(*store.lookup(17)==record);
        check(store.put(copy).status==PutStatus::conflict && store.count()==1);
        auto second=record;second.key=18;check(store.put(second).receipt.row==2);
        auto third=record;third.key=19;check(store.put(third).status==PutStatus::full);
        check(store.put(record).status==PutStatus::accepted && store.put(record).receipt.row==1);
        auto invalid=record;invalid.player_b=invalid.player_a;check(store.put(invalid).status==PutStatus::invalid);
        check(store.count()==2 && !store.lookup(19));
        MemoryResults<1> concurrent;std::atomic<unsigned> accepted{0};std::vector<std::thread> workers;
        for(unsigned i=0;i<24;++i)workers.emplace_back([&]{
            const auto r=concurrent.put(record);
            if(r.status==PutStatus::accepted && r.receipt.row==1)++accepted;
        });
        for(auto& worker:workers)worker.join();
        check(accepted==24 && concurrent.count()==1 && *concurrent.lookup(17)==record);
        const auto json=record_json(record);check(parse_record(json.dump())==std::optional<study_net::MatchRecord>(record));
        check(parse_record(json.dump(2)).has_value());
        for(const char* name:{"key","round","player_a","player_b","ticks","score_a","score_b","lines_a","lines_b","winner"}) {
            for(const Json& wrong:{Json(-1),Json(true),Json("1"),Json(1.5),Json(nullptr)}){
                auto bad=json;bad[name]=wrong;check(!parse_record(bad.dump()));
            }
            auto missing=json;missing.erase(name);check(!parse_record(missing.dump()));
        }
        auto bad=json;bad["lines_a"]=UINT64_MAX;check(!parse_record(bad.dump()));
        bad=json;bad["winner"]=3;check(!parse_record(bad.dump()));
        check(!parse_record(json.dump()+"x"));check(!parse_record(std::string(1025,' ')));
        auto text=json.dump();text.insert(1,"\"key\":17,");check(!parse_record(text));
        check(!object("{\"nested\":{}}",1));check(!object("[]",0));
        auto large=record;large.key=UINT64_MAX;large.score_a=UINT64_MAX;
        check(parse_record(record_json(large).dump())==std::optional<study_net::MatchRecord>(large));
        const auto receipt=parse_receipt(receipt_json(first.receipt).dump());
        check(receipt && receipt->key==17 && receipt->row==1);
        check(!parse_receipt("{\"key\":17,\"row\":0,\"player_a\":101,\"player_b\":202}"));
        std::puts("meta service: atomic duplicate/full handling, 24 workers, value copies and bounded wire parsing");
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
