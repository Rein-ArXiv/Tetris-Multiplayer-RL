#include "meta/sqlite_results.h"
#include "meta/progression_levels.h"
#include <climits>
#include <filesystem>
#include <iostream>
using namespace study_meta;
using study_net::MatchRecord;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
static MatchRecord record(std::uint64_t key=17) {return {key,1,101,202,11,100,0,0,0,MatchRecord::a};}
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string path=argv[1];CHECK(!std::filesystem::exists(path));
    CHECK(expected_score(INT_MAX,INT_MIN)>0.999 && expected_score(INT_MIN,INT_MAX)<0.001);
    CHECK(std::abs(expected_score(100,500)-1.0/11)<1e-12);
    CHECK(std::abs(expected_score(100,500)+expected_score(500,100)-1)<1e-12);
    for(int rp:{0,299,300,599,600,INT_MAX}) {
        const auto pair=progress_for(rp,rp,MatchRecord::a);
        CHECK(pair.a.after>=rp && pair.b.after<=rp && pair.b.after>=0);
    }
    CHECK(progress_for(INT_MAX,INT_MAX,MatchRecord::a).a.after==INT_MAX);
    CHECK(progress_for(INT_MAX,INT_MAX,MatchRecord::a).b.after==INT_MAX-8);
    CHECK(progress_for(299,300,MatchRecord::a).a.after==315);
    CHECK(progress_for(299,300,MatchRecord::a).b.after==288);
    int sum=0;
    for(int level=1;level<=60;++level) {
        CHECK(levels::total_xp_for_level(level)==sum);
        CHECK(levels::level_for_xp(sum)==level);
        if(sum)CHECK(levels::level_for_xp(sum-1)==level-1);
        if(level<60){CHECK(levels::xp_to_next(level)==100+20*(level-1));sum+=100+20*(level-1);}
    }
    CHECK(sum==40120 && levels::total_xp_for_level(INT_MAX)==40120);
    CHECK(levels::total_xp_for_level(INT_MIN)==0 && levels::xp_to_next(INT_MAX)==0);
    CHECK(levels::progress(INT_MIN).into==0 && levels::progress(INT_MIN).need==100);
    CHECK(levels::progress(INT_MAX).level==60 && levels::progress(INT_MAX).need==0);
    CHECK(levels::progress(219).level==2 && levels::progress(219).into==119);
    CHECK(levels::progress(220).level==3 && levels::progress(220).into==0);
    for(auto point:{RewardPoint::after_progress_a,RewardPoint::after_progress_b}) {
        SqliteResults db(":memory:");db.seed_demo();
        CHECK(db.put(record(),[&](RewardPoint at){if(at==point)throw std::runtime_error("injected");}).status==SqlStatus::storage_error);
        CHECK(db.count()==0 && db.balance(101)==0 && db.career(101).xp==0 && db.career(202).xp==0);
        CHECK(!db.progress(17) && !db.awards(17));
    }
    {
        SqliteResults db(path);db.seed_demo();SqliteDb raw(path);
        raw.exec("UPDATE careers SET xp=2147483647 WHERE player_id='202';");
        CHECK(db.put(record()).status==SqlStatus::storage_error);
        CHECK(db.count()==0 && db.balance(101)==0 && db.balance(202)==0);
        CHECK(db.career(101).rp==0 && db.career(101).xp==0 && db.career(202).xp==INT_MAX);
        CHECK(schema_integer(raw,"SELECT count(*) FROM match_progress")==0);
        raw.exec("UPDATE careers SET xp=0 WHERE player_id='202';");
        CHECK(db.put(record()).status==SqlStatus::accepted);
        CHECK(db.career(101).rp==16 && db.career(101).xp==100);
        CHECK(db.career(202).rp==0 && db.career(202).xp==50);
        const auto first=*db.progress(17);
        CHECK(db.put(record(18)).status==SqlStatus::accepted);
        const auto before=db.career(101);CHECK(db.put(record()).status==SqlStatus::accepted);
        CHECK(db.career(101).rp==before.rp && db.career(101).xp==200);
        CHECK(db.progress(17)->a.after==first.a.after && db.progress(17)->a.xp==100);
        auto draw=record(19);draw.winner=MatchRecord::draw;
        CHECK(db.put(draw).status==SqlStatus::accepted && db.career(101).rp==before.rp && db.career(101).xp==200);
        CHECK(db.progress(19)->a.before==db.progress(19)->a.after && db.progress(19)->a.xp==0);
        raw.exec("DELETE FROM match_progress WHERE match_id=1 AND player_id='202';");
        CHECK(db.put(record()).status==SqlStatus::storage_error && db.career(101).xp==200);
        raw.exec("INSERT INTO match_progress VALUES(1,'202',0,0,50,1);");
    }
    {SqliteResults db(path);CHECK(db.career(101).xp==200 && db.progress(17)->a.after==16);}
    {
        SqliteDb raw(":memory:");raw.exec("PRAGMA foreign_keys=ON;");
        raw.exec(kStudySchema);raw.exec(kMigrationTable);raw.exec(kSelectionColumn);raw.exec(kRewardSchema);
        raw.exec("INSERT INTO icons VALUES('default','Default');"
                 "INSERT INTO players(id,display_name) VALUES('101','A'),('202','B');"
                 "INSERT INTO player_icons VALUES('101','default'),('202','default');"
                 "INSERT INTO wallets VALUES('101',10),('202',3);"
                 "INSERT INTO matches VALUES(1,'17','1','101','202','101','11','100','0',0,0);"
                 "INSERT INTO match_rewards VALUES(1,'101',10,1),(1,'202',3,1);"
                 "INSERT INTO study_schema_history VALUES(1,'base_tables_v1'),(2,'default_selection_v2'),(3,'atomic_rewards_v3');"
                 "PRAGMA user_version=3; PRAGMA application_id=1413829714;");
        migrate_study(raw);CHECK(recorded_version(raw)==4);
        CHECK(schema_integer(raw,"SELECT sum(bp) FROM wallets")==13);
        CHECK(schema_integer(raw,"SELECT sum(xp)+sum(rp) FROM careers")==0);
        CHECK(schema_integer(raw,"SELECT count(*) FROM match_progress WHERE policy=0 AND xp_delta=0")==2);
        CHECK(schema_integer(raw,"SELECT count(*) FROM match_rewards WHERE policy=1")==2);
    }
    std::cout<<"RP boundaries, all level thresholds, XP rollback, retry snapshots, draw, restart and v3 migration: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
