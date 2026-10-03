#include "meta/query_observer.h"
#include "meta/sqlite_results.h"
#include <algorithm>
#include <iostream>
#include <functional>
#include <array>
using namespace study_meta;
#define CHECK(x) do {if (!(x)) throw std::runtime_error("check failed: " #x);} while(false)
using Rows = std::vector<std::vector<std::optional<std::string>>>;
static void rejects(const std::function<void()>& action) {
    bool threw = false;
    try {action();} catch (const std::exception&) {threw = true;}
    CHECK(threw);
}
static void report(const char* label, const QueryObservation& q) {
    std::cout << label << ": rows=" << q.rows.size() << " fullscan=" << q.fullscan_steps
              << " sorts=" << q.sorts << " vm=" << q.vm_steps << '\n';
    for (const auto& detail : q.plan) std::cout << "  " << detail << '\n';
}
static QueryBind history_bind(unsigned player, unsigned limit) {
    return [=](sqlite3_stmt* s) {
        bind_observer_text(s, 1, std::to_string(player));
        bind_observer_int(s, 2, limit);
    };
}
static sqlite3_int64 pages(SqliteDb& db) {
    Statement q(db.get(), "PRAGMA page_count");CHECK(q.next());return q.integer_column(0);
}
int main() {
 try {
    SqliteDb db(":memory:");db.exec("PRAGMA foreign_keys=ON;");db.exec(kStudySchema);
    db.exec("BEGIN;");
    for (unsigned id=1; id<=64; ++id) {
        Statement player(db.get(), "INSERT INTO players VALUES(?1,'Player')");
        player.text(1, std::to_string(id));player.done();
    }
    db.exec("INSERT INTO icons VALUES('default','Default'),('ruby','Ruby');");
    for (unsigned id=1; id<=64; ++id) {
        Statement icon(db.get(), "INSERT INTO player_icons VALUES(?1,'ruby')");
        icon.text(1, std::to_string(id));icon.done();
    }
    Rows expected;
    std::array<Rows,65> by_player;
    for (unsigned i=1; i<=12000; ++i) {
        const unsigned a=i%64+1, b=(i+17)%64+1;
        const auto key=std::to_string(UINT64_MAX-i);
        Statement q(db.get(),
            "INSERT INTO matches(match_key,round,player_a,player_b,winner,ticks,score_a,score_b,lines_a,lines_b) "
            "VALUES(?1,'1',?2,?3,NULL,'11','0','0',0,0)");
        q.text(1,key);q.text(2,std::to_string(a));q.text(3,std::to_string(b));q.done();
        by_player[a].push_back({std::to_string(i),key});
        by_player[b].push_back({std::to_string(i),key});
        if(a==17 || b==17) expected.push_back({std::to_string(i),key});
    }
    db.exec("COMMIT;");
    std::reverse(expected.begin(),expected.end());expected.resize(5);
    const auto bind=history_bind(17,5);
    const auto before=observe_query(db.get(),kRecentMatches,bind);
    const auto or_before=observe_query(db.get(),kRecentMatchesOr,bind);
    CHECK(before.rows==expected && or_before.rows==expected);
    const auto pages_before=pages(db);
    db.exec(kHistoryIndexes);
    const auto pages_after=pages(db);
    const auto after=observe_query(db.get(),kRecentMatches,bind);
    const auto or_after=observe_query(db.get(),kRecentMatchesOr,bind);
    CHECK(after.rows==expected && or_after.rows==expected);
    CHECK(after.fullscan_steps==0 && after.sorts==0);
    CHECK(before.fullscan_steps>after.fullscan_steps && before.vm_steps>after.vm_steps);
    CHECK(pages_after>pages_before);
    for(unsigned player=1; player<=64; ++player) {
        std::reverse(by_player[player].begin(),by_player[player].end());
        for(unsigned limit : {1U,5U,50U}) {
            auto want=by_player[player];want.resize(limit);
            CHECK(observe_query(db.get(),kRecentMatches,history_bind(player,limit)).rows==want);
        }
    }
    report("history before",before);report("history indexed",after);report("OR indexed",or_after);
    std::cout << "fixture pages " << pages_before << " -> " << pages_after << '\n';
    // Statistics can change the chosen algorithm; the result contract is unchanged.
    db.exec("ANALYZE;");
    CHECK(observe_query(db.get(),kRecentMatches,bind).rows==expected);
    CHECK(observe_query(db.get(),kRecentMatches,history_bind(999,5)).rows.empty());
    const auto icons=observe_query(db.get(),
        "SELECT icon_id FROM player_icons WHERE player_id=?1 ORDER BY icon_id",
        [](sqlite3_stmt* s){bind_observer_text(s,1,"17");});
    CHECK((icons.rows==Rows{{std::string("ruby")}}) && icons.sorts==0 && icons.fullscan_steps==0);
    report("ownership composite PK",icons);
    db.exec("CREATE INDEX redundant_pid ON player_icons(player_id);");
    const auto with_extra=observe_query(db.get(),
        "SELECT 1 FROM player_icons WHERE player_id=?1 AND icon_id=?2",
        [](sqlite3_stmt* s){bind_observer_text(s,1,"17");bind_observer_text(s,2,"ruby");});
    db.exec("DROP INDEX redundant_pid;");
    const auto without_extra=observe_query(db.get(),
        "SELECT 1 FROM player_icons WHERE player_id=?1 AND icon_id=?2",
        [](sqlite3_stmt* s){bind_observer_text(s,1,"17");bind_observer_text(s,2,"ruby");});
    CHECK(with_extra.rows==without_extra.rows && without_extra.fullscan_steps==0);
    rejects([&]{db.exec("INSERT INTO player_icons VALUES('17','ruby')");});
    // An ordinary index and a UNIQUE index have different admissible inputs.
    db.exec("CREATE TABLE uniqueness_demo(code TEXT); CREATE INDEX normal_code ON uniqueness_demo(code);"
            "INSERT INTO uniqueness_demo VALUES('x'),('x');");
    rejects([&]{db.exec("CREATE UNIQUE INDEX unique_code ON uniqueness_demo(code)");});
    db.exec("DELETE FROM uniqueness_demo; DROP INDEX normal_code;"
            "CREATE UNIQUE INDEX unique_code ON uniqueness_demo(code);"
            "INSERT INTO uniqueness_demo VALUES('x'),(NULL),(NULL);");
    rejects([&]{db.exec("INSERT INTO uniqueness_demo VALUES('x')");});
    // Wrong leading columns: TEXT numeric order and skipped-prefix assumptions stay visible.
    auto text_order=observe_query(db.get(),"SELECT id FROM players ORDER BY id LIMIT 3",{});
    CHECK((text_order.rows==Rows{{std::string("1")},{std::string("10")},{std::string("11")}}));
    auto nulls=observe_query(db.get(),"SELECT NULL,42,''",{});
    CHECK(!nulls.rows[0][0] && nulls.rows[0][1]=="42" && nulls.rows[0][2]=="");
    rejects([&]{observe_query(db.get(),"SELECT ?1",[](sqlite3_stmt* s){bind_observer_int(s,2,1);});});
    CHECK(observe_query(db.get(),"SELECT 1",{}).rows.size()==1);
    // Exercise the same public API used by history_list, not only equivalent raw SQL.
    SqliteResults store(":memory:");store.seed_demo();
    study_net::MatchRecord a{9,1,101,202,11,1,0,0,0,study_net::MatchRecord::a};
    CHECK(store.put(a).status==SqlStatus::accepted);
    a.key=3;std::swap(a.player_a,a.player_b);CHECK(store.put(a).status==SqlStatus::accepted);
    const auto recent=store.recent(101,1);
    CHECK(recent.size()==1 && recent[0].row==2 && recent[0].key==3);
    CHECK(store.recent(202,50).size()==2 && store.recent(303,5).empty());
    rejects([&]{store.recent(0,5);});rejects([&]{store.recent(101,0);});rejects([&]{store.recent(101,51);});
    // Removing performance indexes preserves data and uniqueness; the public constructor recreates them on reopen.
    db.exec("DROP INDEX idx_study_matches_a_recent; DROP INDEX idx_study_matches_b_recent;");
    CHECK(observe_query(db.get(),kRecentMatches,bind).rows==expected);
    std::cout << "history, plans, bounded results, UNIQUE, NULL, and index independence: passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
