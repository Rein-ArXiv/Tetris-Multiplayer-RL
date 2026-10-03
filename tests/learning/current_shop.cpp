#include "third_party/sqlite3.h"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>
#include <string>
#include <filesystem>
#include <iostream>
static bool race=false, inject_owner_error=false;
static std::mutex barrier_mutex;static std::condition_variable barrier_cv;static unsigned arrived=0;
static int shop_step(sqlite3_stmt* statement) {
    const char* sql=sqlite3_sql(statement);
    const bool owner=sql && std::string(sql).find("SELECT 1 FROM player_icons WHERE player_id=?1 AND icon_id=?2")!=std::string::npos;
    if(owner && inject_owner_error){inject_owner_error=false;return SQLITE_IOERR;}
    const int rc=sqlite3_step(statement);
    if(owner && race && rc==SQLITE_DONE && sqlite3_get_autocommit(sqlite3_db_handle(statement))) {
        std::unique_lock<std::mutex> lock(barrier_mutex);++arrived;barrier_cv.notify_all();
        if(!barrier_cv.wait_for(lock,std::chrono::seconds(3),[]{return arrived==2;}))return SQLITE_BUSY;
    }
    return rc;
}
#define sqlite3_step shop_step
#include "meta/database.cpp"
#undef sqlite3_step
#include "meta/sqlite_handle.h"
using namespace study_meta;
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string prefix=argv[1];unsigned failures=0;
    for(const auto mode:{"race","error"}) {
        const auto path=prefix+mode;CHECK(!std::filesystem::exists(path));
        meta::Database first(path);CHECK(first.registerGuest(std::string(32,'a')));meta::Database second(path);SqliteDb raw(path);
        raw.exec("UPDATE players SET bp=300;");
        std::optional<meta::Player> a,b;
        if(std::string(mode)=="race") {
            meta::IconPurchaseResult left{},right{};race=true;
            std::thread t1([&]{left=first.purchaseIcon(std::string(32,'a'),"ruby",a);});
            std::thread t2([&]{right=second.purchaseIcon(std::string(32,'a'),"ruby",b);});t1.join();t2.join();race=false;
            const bool statuses=(left==meta::IconPurchaseResult::Ok && right==meta::IconPurchaseResult::AlreadyOwned) ||
                (right==meta::IconPurchaseResult::Ok && left==meta::IconPurchaseResult::AlreadyOwned);
            const auto p=first.getByToken(std::string(32,'a'));const bool ok=statuses && p && p->bp==200;
            std::cout<<"concurrent purchase charged once: "<<ok<<" balance="<<p->bp<<'\n';failures+=!ok;
        }else {
            inject_owner_error=true;const auto result=first.purchaseIcon(std::string(32,'a'),"ruby",a);
            const auto p=first.getByToken(std::string(32,'a'));
            const bool ok=result==meta::IconPurchaseResult::DbError && p && p->bp==300 && !a;
            std::cout<<"ownership read error does not charge: "<<ok<<'\n';failures+=!ok;
        }
    }
    {
        const auto path=prefix+"ignored";meta::Database db(path);CHECK(db.registerGuest(std::string(32,'a')));SqliteDb raw(path);
        raw.exec("UPDATE players SET bp=100; CREATE TRIGGER ignore_grant BEFORE INSERT ON player_icons WHEN NEW.icon_id='ruby' BEGIN SELECT RAISE(IGNORE); END;");
        std::optional<meta::Player> out;
        CHECK(db.purchaseIcon(std::string(32,'a'),"ruby",out)==meta::IconPurchaseResult::DbError && !out);
        CHECK(db.getByToken(std::string(32,'a'))->bp==100);
        raw.exec("DROP TRIGGER ignore_grant;");
        inject_owner_error=true;CHECK(db.selectIcon(std::string(32,'a'),"ruby",out)==meta::IconSelectResult::DbError && !out);
        CHECK(db.purchaseIcon(std::string(32,'a'),"ruby",out)==meta::IconPurchaseResult::Ok && out->bp==0);
        CHECK(db.selectIcon(std::string(32,'a'),"ruby",out)==meta::IconSelectResult::Ok && out->selected_icon_id=="ruby");
        CHECK(db.selectIcon(std::string(32,'a'),"ruby",out)==meta::IconSelectResult::Ok && out->bp==0);
    }
    return failures?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
