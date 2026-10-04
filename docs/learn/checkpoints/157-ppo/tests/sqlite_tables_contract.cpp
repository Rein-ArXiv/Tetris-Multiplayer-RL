#include "meta/sqlite_results.h"
#include <filesystem>
#include <iostream>
#include <thread>
#include <array>
#include <functional>
using namespace study_meta;
using study_net::MatchRecord;
#define CHECK(x) do { if(!(x))throw std::runtime_error("check failed: " #x); } while(false)
static void rejects(const std::function<void()>& action) {
    bool threw=false;
    try {action();} catch(const std::exception&) {threw=true;}
    CHECK(threw);
}
// Only delete a fixture that did not exist before the test.
struct FixturePath {
    std::string path;
    explicit FixturePath(const char* p):path(p) {
        for(const auto* suffix:{"","-journal","-wal","-shm"})
            if(std::filesystem::exists(path+suffix))throw std::runtime_error("fixture path already exists");
    }
    ~FixturePath() {
        for(const auto* suffix:{"","-journal","-wal","-shm"}) {
            std::error_code error;std::filesystem::remove(path+suffix,error);
        }
    }
};
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);FixturePath fixture(argv[1]);
    MatchRecord record{17,1,101,202,11,100,0,0,0,MatchRecord::a};
    std::uint64_t first_row=0;
    {
        SqliteResults store(fixture.path);store.seed_demo();store.seed_demo();
        CHECK(store.count()==0 && !store.lookup(17));
        auto result=store.put(record);CHECK(result.status==SqlStatus::accepted);
        first_row=result.receipt.row;CHECK(first_row>0);
        CHECK(store.put(record).receipt.row==first_row);
        auto different=record;different.score_a++;
        CHECK(store.put(different).status==SqlStatus::conflict && store.count()==1);
        auto copy=store.lookup(17);CHECK(copy && copy->record==record);copy->record.score_a++;
        CHECK(store.lookup(17)->record==record);
        CHECK(store.rename_player(101,"shared name"));store.add_player(303,"shared name");
        CHECK(!store.rename_player(999,"absent"));CHECK(store.lookup(17)->record.player_a==101);
        CHECK(store.grant_icon(101,"ruby") && !store.grant_icon(101,"ruby"));
        CHECK((store.icons(101)==std::vector<std::string>{"default","ruby"}));
        rejects([&]{store.grant_icon(999,"default");});rejects([&]{store.grant_icon(101,"unknown");});
        rejects([&]{store.add_player(0,"invalid");});rejects([&]{store.add_player(101,"duplicate");});
        store.add_player(UINT64_MAX,"'); DROP TABLE matches; --");
        auto wide=record;wide.key=wide.round=wide.ticks=wide.score_a=wide.score_b=UINT64_MAX;
        wide.player_a=UINT64_MAX;wide.lines_a=wide.lines_b=UINT32_MAX;wide.winner=MatchRecord::draw;
        CHECK(store.put(wide).status==SqlStatus::accepted && store.lookup(wide.key)->record==wide);
        auto invalid=record;invalid.key=18;invalid.player_a=999;CHECK(store.put(invalid).status==SqlStatus::invalid);
        invalid=record;invalid.key=0;CHECK(store.put(invalid).status==SqlStatus::invalid);
        std::array<SqlResult,16> replies{};std::vector<std::thread> threads;
        auto parallel=record;parallel.key=19;
        for(std::size_t i=0;i<replies.size();++i)threads.emplace_back([&,i]{replies[i]=store.put(parallel);});
        for(auto& thread:threads)thread.join();
        for(const auto& reply:replies)CHECK(reply.status==SqlStatus::accepted && reply.receipt.row==replies[0].receipt.row);
        CHECK(store.count()==3);
    }
    {
        SqliteResults reopened(fixture.path);reopened.seed_demo();CHECK(reopened.count()==3);
        CHECK(reopened.put(record).receipt.row==first_row);
        CHECK(reopened.lookup(UINT64_MAX)->record.score_a==UINT64_MAX);
        CHECK(reopened.lookup(UINT64_MAX)->record.winner==MatchRecord::draw);
        CHECK((reopened.icons(101)==std::vector<std::string>{"default","ruby"}));
    }
    {
        SqliteDb raw(fixture.path);raw.exec("PRAGMA foreign_keys=ON;");
        Statement fk(raw.get(),"PRAGMA foreign_keys");CHECK(fk.next() && fk.integer_column(0)==1);
        for(const auto& id:std::vector<std::string>{"","0","01","-1","+1","1.5","18446744073709551616",std::string("4\0x",3)})
            rejects([&]{Statement q(raw.get(),"INSERT INTO players(id,display_name) VALUES(?1,'x')");q.text(1,id);q.done();});
        for(const auto* sql:{
            "INSERT INTO players(id,display_name) VALUES(NULL,'x')",
            "INSERT INTO players(id,display_name) VALUES('404',NULL)",
            "INSERT INTO players(id,display_name) VALUES(x'343034','x')",
            "UPDATE matches SET player_a=player_b WHERE match_key='17'",
            "UPDATE matches SET winner='303' WHERE match_key='17'",
            "UPDATE matches SET player_b='999' WHERE match_key='17'",
            "UPDATE matches SET lines_a=1.5 WHERE match_key='17'",
            "UPDATE matches SET lines_a=4294967296 WHERE match_key='17'",
            "UPDATE matches SET score_a='-1' WHERE match_key='17'",
            "UPDATE matches SET ticks='0' WHERE match_key='17'",
            "UPDATE matches SET match_key=NULL WHERE match_key='17'",
            "UPDATE matches SET match_key='17' WHERE match_key='19'",
            "DELETE FROM players WHERE id='101'",
            "DELETE FROM icons WHERE id='default'"})rejects([&]{raw.exec(sql);});
        raw.exec("INSERT INTO player_icons VALUES('303','ruby'); DELETE FROM players WHERE id='303';");
        Statement count(raw.get(),"SELECT count(*) FROM player_icons WHERE player_id='303'");CHECK(count.next() && count.integer_column(0)==0);
        Statement name(raw.get(),"SELECT display_name FROM players WHERE id='18446744073709551615'");
        CHECK(name.next() && name.text_column(0)=="'); DROP TABLE matches; --");
        Statement draw(raw.get(),"SELECT winner FROM matches WHERE match_key='18446744073709551615'");CHECK(draw.next() && draw.is_null(0));
        rejects([&]{draw.integer_column(0);});rejects([&]{Statement bad(raw.get(),"SELEC invalid");});
        rejects([&]{Statement bad(raw.get(),"");});rejects([&]{Statement q(raw.get(),"SELECT ?1");q.text(2,"bad index");});
        Statement bytes(raw.get(),"SELECT ?1");bytes.text(1,std::string("a\0b",3));CHECK(bytes.next() && bytes.text_column(0)==std::string("a\0b",3));
        raw.exec("INSERT INTO icons VALUES('after-error','After error');");
    }
    rejects([]{SqliteDb empty("");});rejects([]{SqliteDb nul(std::string("a\0b",3));});
    std::cout<<"SQLite keys, constraints, uint64, NULL, binding, reopen and concurrency: passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
