#include "meta/sqlite_results.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
using namespace study_meta;
using study_net::MatchRecord;
#define CHECK(x) do { if(!(x))throw std::runtime_error("check failed: " #x); } while(false)
static MatchRecord record(std::uint64_t key=17) {return {key,1,101,202,11,100,0,0,0,MatchRecord::a};}
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string path=argv[1];CHECK(!std::filesystem::exists(path));
    CHECK(credited_balance(kBalanceLimit-10,10)==kBalanceLimit);
    for(const auto pair:std::array<std::array<int,2>,3>{{{kBalanceLimit,1},{-1,1},{1,-1}}}) {
        bool caught=false;try{credited_balance(pair[0],pair[1]);}catch(const std::overflow_error&){caught=true;}CHECK(caught);
    }
    for(auto point:{RewardPoint::after_match,RewardPoint::after_a,RewardPoint::after_b,RewardPoint::before_commit}) {
        SqliteResults store(":memory:");store.seed_demo();
        const auto result=store.put(record(),[&](RewardPoint at){if(at==point)throw std::runtime_error("injected");});
        CHECK(result.status==SqlStatus::storage_error && result.receipt.row==0);
        CHECK(store.count()==0 && store.balance(101)==0 && store.balance(202)==0 && !store.awards(17));
        CHECK(store.put(record()).status==SqlStatus::accepted);
        CHECK(store.balance(101)==10 && store.balance(202)==3);
        const auto saved=store.awards(17);CHECK(saved && saved->a==10 && saved->b==3 && saved->policy==1);
        const auto first=store.put(record());CHECK(first.status==SqlStatus::accepted && store.count()==1);
        CHECK(store.balance(101)==10 && store.balance(202)==3);
        auto conflict=record();conflict.score_a++;
        CHECK(store.put(conflict).status==SqlStatus::conflict && store.balance(101)==10);
        auto draw=record(18);draw.winner=MatchRecord::draw;
        CHECK(store.put(draw).status==SqlStatus::accepted && store.awards(18)->a==0);
        CHECK(store.balance(101)==10 && store.balance(202)==3);
        auto unknown=record(19);unknown.player_b=999;
        CHECK(store.put(unknown).status==SqlStatus::invalid && store.count()==2);
    }
    {
        SqliteResults store(path);store.seed_demo();
        SqliteDb raw(path);
        raw.exec("CREATE TRIGGER fail_second BEFORE INSERT ON match_rewards WHEN NEW.player_id='202' "
                 "BEGIN SELECT RAISE(ABORT,'second award failed'); END;");
        CHECK(store.put(record()).status==SqlStatus::storage_error);
        CHECK(store.count()==0 && store.balance(101)==0 && !store.awards(17));
        raw.exec("DROP TRIGGER fail_second; UPDATE wallets SET bp=2147483647 WHERE player_id='202';");
        CHECK(store.put(record()).status==SqlStatus::storage_error);
        CHECK(store.count()==0 && store.balance(101)==0 && store.balance(202)==kBalanceLimit);
        raw.exec("UPDATE wallets SET bp=0 WHERE player_id='202';");
        CHECK(store.put(record()).status==SqlStatus::accepted);
        // A read snapshot sees only the old committed pair, and blocks journal COMMIT.
        SqliteDb reader(path);reader.exec("BEGIN;");
        {
            Statement q(reader.get(),"SELECT bp FROM wallets WHERE player_id='101'");CHECK(q.next() && q.integer_column(0)==10);
            bool reached=false;
            CHECK(store.put(record(18),[&](RewardPoint at){if(at==RewardPoint::before_commit)reached=true;}).status==SqlStatus::storage_error);
            CHECK(reached && store.count()==1 && store.balance(101)==10 && store.balance(202)==3);
        }
        reader.exec("ROLLBACK;");
    }
    constexpr unsigned n=8;
    std::array<std::unique_ptr<SqliteResults>,n> connections;
    for(auto& p:connections)p=std::make_unique<SqliteResults>(path);
    std::array<SqlResult,n> result{};std::vector<std::thread> threads;
    for(unsigned i=0;i<n;++i)threads.emplace_back([&,i]{result[i]=connections[i]->put(record(100+i));});
    for(auto& t:threads)t.join();
    for(const auto& r:result)CHECK(r.status==SqlStatus::accepted);
    CHECK(connections[0]->balance(101)==90 && connections[0]->balance(202)==27);
    threads.clear();
    for(unsigned i=0;i<n;++i)threads.emplace_back([&,i]{result[i]=connections[i]->put(record(200));});
    for(auto& t:threads)t.join();
    for(const auto& r:result)CHECK(r.status==SqlStatus::accepted && r.receipt.row==result[0].receipt.row);
    CHECK(connections[0]->balance(101)==100 && connections[0]->balance(202)==30);
    {SqliteResults reopened(path);CHECK(reopened.count()==10 && reopened.balance(101)==100);}
    std::cout<<"Atomic result/two awards, faults, SQL abort, overflow, COMMIT busy, retry, draw, independent connections and reopen: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
