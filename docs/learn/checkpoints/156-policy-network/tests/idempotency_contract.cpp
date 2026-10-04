#include "meta/sqlite_results.h"
#include "meta/settlement_wire.h"
#include <filesystem>
#include <iostream>
using namespace study_meta;
using study_net::MatchRecord;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
static MatchRecord record(std::uint64_t key=17) {return {key,1,101,202,11,100,0,0,0,MatchRecord::a};}
static Json receipt(const SqlResult& r) {
    return settlement_json({r.receipt,static_cast<std::uint32_t>(r.awards.a),static_cast<std::uint32_t>(r.awards.b),static_cast<std::uint32_t>(r.awards.policy)});
}
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string path=argv[1];CHECK(!std::filesystem::exists(path));
    Json original;
    {
        SqliteResults store(path);store.seed_demo();
        const auto first=store.put(record());CHECK(first.status==SqlStatus::accepted);original=receipt(first);
        CHECK(original["bp_a"]==10 && original["bp_b"]==3 && original["policy"]==1);
        CHECK(store.put(record(18)).status==SqlStatus::accepted && store.balance(101)==20);
        CHECK(receipt(store.put(record()))==original);
        const auto body=record_json(record());
        for(const auto* field:{"round","player_a","player_b","ticks","score_a","score_b","lines_a","lines_b","winner"}) {
            auto changed=body;changed[field]=changed[field].get<std::uint64_t>()+1;
            const auto parsed=parse_record(changed.dump());CHECK(parsed);
            CHECK(store.put(*parsed).status==SqlStatus::conflict);
            CHECK(store.balance(101)==20 && store.balance(202)==6 && store.count()==2);
        }
        auto other_key=record(19);CHECK(store.put(other_key).status==SqlStatus::accepted);
        CHECK(store.count()==3 && store.balance(101)==30); // New identity means a different operation.
        auto reordered=Json::parse("{\"winner\":1,\"lines_b\":0,\"lines_a\":0,\"score_b\":0,\"score_a\":100,\"ticks\":11,\"player_b\":202,\"player_a\":101,\"round\":1,\"key\":17}");
        CHECK(*parse_record(reordered.dump(2))==record());
        CHECK(receipt(store.put(*parse_record(reordered.dump(2))))==original);
    }
    {
        SqliteResults reopened(path);CHECK(receipt(reopened.put(record()))==original);
        // An incomplete stored receipt must not be silently re-created or rewarded.
        SqliteDb raw(path);raw.exec("DELETE FROM match_rewards WHERE match_id=1 AND player_id='202';");
        CHECK(reopened.put(record()).status==SqlStatus::storage_error);
        CHECK(reopened.balance(101)==30 && reopened.balance(202)==9);
        raw.exec("INSERT INTO match_rewards VALUES(1,'202',3,1);");
    }
    CHECK(parse_settlement(original.dump()));
    auto wide=original;wide["key"]=UINT64_MAX;wide["row"]=UINT64_MAX;wide["player_a"]=UINT64_MAX;
    const auto parsed=parse_settlement(wide.dump());CHECK(parsed && parsed->match.key==UINT64_MAX && parsed->match.row==UINT64_MAX && parsed->match.player_a==UINT64_MAX);
    for(const auto* field:{"key","row","player_a","player_b","bp_a","bp_b","policy"}) {
        auto bad=original;bad[field]=-1;CHECK(!parse_settlement(bad.dump()));
        bad=original;bad[field]=1.5;CHECK(!parse_settlement(bad.dump()));
        bad=original;bad.erase(field);CHECK(!parse_settlement(bad.dump()));
    }
    for(auto change:std::vector<Json>{{{"policy",2}},{{"bp_a",11}},{{"bp_a",5}},{{"policy",0}},{{"key",0}},{{"player_b",101}}}) {
        auto bad=original;bad.update(change);CHECK(!parse_settlement(bad.dump()));
    }
    auto legacy=original;legacy["policy"]=0;legacy["bp_a"]=legacy["bp_b"]=0;CHECK(parse_settlement(legacy.dump()));
    auto draw=legacy;draw["policy"]=1;CHECK(parse_settlement(draw.dump()));
    auto duplicate=original.dump();duplicate.insert(1,"\"key\":17,");CHECK(!parse_settlement(duplicate));
    CHECK(!parse_settlement(std::string(1025,' ')));
    std::cout<<"Original receipt after new matches/reopen, every input conflict, semantic JSON, missing ledger, full uint64 and malformed receipts: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
