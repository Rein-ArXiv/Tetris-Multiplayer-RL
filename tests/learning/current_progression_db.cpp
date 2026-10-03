#include "meta/database.h"
#include "meta/sqlite_handle.h"
#include <filesystem>
#include <iostream>
using study_meta::SqliteDb;
using study_meta::Statement;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string path=argv[1];CHECK(!std::filesystem::exists(path));
    meta::Database db(path);
    const auto a=db.registerGuest(std::string(32,'a')),b=db.registerGuest(std::string(32,'b'));CHECK(a && b);
    meta::MatchRecord record{std::string(32,'c'),a->id,b->id,a->id,100,0,1,0,60};
    SqliteDb raw(path);
    for(const auto* value:{"2147483648","-1","3.5","'oops'"}) {
        raw.exec((std::string("UPDATE players SET elo=")+value+" WHERE id="+std::to_string(a->id)).c_str());
        meta::MatchSaveError error;CHECK(!db.saveMatch(record,&error) && error==meta::MatchSaveError::Database);
        CHECK(!db.getByToken(std::string(32,'a')) && !db.leaderboard(10));
        Statement q(raw.get(),"SELECT count(*) FROM matches");CHECK(q.next() && q.integer_column(0)==0);
        Statement sums(raw.get(),"SELECT sum(bp),sum(xp) FROM players");CHECK(sums.next() && sums.integer_column(0)==0 && sums.integer_column(1)==0);
    }
    raw.exec("UPDATE players SET elo=2147483647;");
    const auto result=db.saveMatch(record);CHECK(result && result->a.elo_after==2147483647 && result->b.elo_after==2147483639);
    CHECK(db.getByToken(std::string(32,'a'))->xp==100);
    CHECK(db.getByToken(std::string(32,'b'))->xp==50);
    raw.exec("UPDATE matches SET elo_a_before=4294967296;");
    meta::MatchSaveError error;CHECK(!db.saveMatch(record,&error) && error==meta::MatchSaveError::Database);
    CHECK(db.getByToken(std::string(32,'a'))->xp==100);
    std::cout<<"Actual DB rejects RP type/range and malformed snapshots; maximum RP settles without wrap: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
