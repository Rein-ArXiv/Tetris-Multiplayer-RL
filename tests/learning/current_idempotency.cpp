// Inject only the production implementation's duplicate-lookup step.
#include "third_party/sqlite3.h"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>
#include <string>
#include <filesystem>
#include <iostream>
static bool inject_read_error=false, race_enabled=false;
static std::mutex barrier_mutex;
static std::condition_variable barrier_cv;
static unsigned arrived=0;
static bool is_lookup(sqlite3_stmt* statement) {
    const char* sql=sqlite3_sql(statement);
    return sql && std::string(sql).find("FROM matches WHERE match_uuid=?1")!=std::string::npos;
}
static int idempotency_step(sqlite3_stmt* statement) {
    const bool target=is_lookup(statement);
    if(target && inject_read_error){inject_read_error=false;return SQLITE_IOERR;}
    const int result=sqlite3_step(statement);
    // Reproduce the old check-then-write race only when the lookup is outside a transaction.
    if(target && race_enabled && result==SQLITE_DONE && sqlite3_get_autocommit(sqlite3_db_handle(statement))) {
        std::unique_lock<std::mutex> lock(barrier_mutex);++arrived;barrier_cv.notify_all();
        if(!barrier_cv.wait_for(lock,std::chrono::seconds(3),[]{return arrived==2;}))return SQLITE_BUSY;
    }
    return result;
}
#define sqlite3_step idempotency_step
#include "meta/database.cpp"
#undef sqlite3_step
#include "meta/sqlite_handle.h"
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
static meta::MatchRecord fixture(meta::Database& db) {
    const auto a=db.registerGuest(std::string(32,'a')),b=db.registerGuest(std::string(32,'b'));
    CHECK(a && b);
    return {std::string(32,'c'),a->id,b->id,a->id,100,0,1,0,60};
}
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string prefix=argv[1];unsigned failures=0;
    for(const auto* suffix:{"error.db","race.db"})CHECK(!std::filesystem::exists(prefix+suffix));
    {
        const auto path=prefix+"error.db";meta::Database db(path);auto r=fixture(db);
        inject_read_error=true;meta::MatchSaveError error;const auto result=db.saveMatch(r,&error);
        study_meta::SqliteDb raw(path);
        study_meta::Statement q(raw.get(),"SELECT count(*) FROM matches");CHECK(q.next());
        const bool ok=!result && error==meta::MatchSaveError::Database && q.integer_column(0)==0;
        std::cout<<"lookup I/O error aborts without a match: "<<ok<<'\n';failures+=!ok;
        const auto retried=db.saveMatch(r);CHECK(retried);
    }
    {
        const auto path=prefix+"race.db";meta::Database first(path);auto r=fixture(first);meta::Database second(path);
        std::optional<meta::MatchInsertResult> a,b;race_enabled=true;
        std::thread left([&]{a=first.saveMatch(r);});std::thread right([&]{b=second.saveMatch(r);});
        left.join();right.join();race_enabled=false;
        const bool same=a && b && a->match_id==b->match_id && a->a.elo_after==b->a.elo_after;
        std::cout<<"two connections return the same receipt: "<<same<<'\n';failures+=!same;
        study_meta::SqliteDb raw(path);
        study_meta::Statement count(raw.get(),"SELECT count(*) FROM matches");CHECK(count.next() && count.integer_column(0)==1);
        study_meta::Statement balances(raw.get(),"SELECT sum(bp),sum(xp),sum(wins),sum(losses) FROM players");
        CHECK(balances.next() && balances.integer_column(0)==40 && balances.integer_column(1)==150 && balances.integer_column(2)==1 && balances.integer_column(3)==1);
    }
    return failures ? 1 : 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
