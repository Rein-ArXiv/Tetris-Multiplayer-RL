// Faults affect only calls from the included production DB implementation.
#include "third_party/sqlite3.h"
#include <string>
#include <stdexcept>
#include <filesystem>
#include <iostream>
static std::string fault_sql;
static bool fired=false;
static int migration_step(sqlite3_stmt* statement) {
    const char* sql=sqlite3_sql(statement);
    if(!fired && sql && fault_sql==sql) {fired=true;return SQLITE_IOERR;}
    return sqlite3_step(statement);
}
#define sqlite3_step migration_step
#include "meta/database.cpp"
#undef sqlite3_step
#include "meta/sqlite_handle.h"
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);} while(false)
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);
    const std::string base=argv[1];
    const std::pair<const char*,const char*> cases[]={
        {"PRAGMA user_version","version read failed"},
        {"SELECT 1 FROM schema_migrations WHERE name='elo_to_rp_v1'","marker read failed"},
        {"PRAGMA table_info(players)","schema inspection read failed"},
        {"SELECT 1 FROM schema_migrations WHERE name='credential_scrub_v1'","scrub marker read failed"}
    };
    unsigned failures=0;
    for(unsigned i=0;i<4;++i) {
        const auto path=base+std::to_string(i)+".db";
        CHECK(!std::filesystem::exists(path));
        fault_sql.clear();fired=false;
        {meta::Database db(path);CHECK(db.registerGuest(std::string(32,'a')));}
        {
            study_meta::SqliteDb db(path);
            db.exec("UPDATE players SET elo=1500; PRAGMA user_version=1;");
            if(i==0)db.exec("DELETE FROM schema_migrations WHERE name='elo_to_rp_v1';");
        }
        fault_sql=cases[i].first;fired=false;
        std::string error;
        try {meta::Database db(path);} catch(const std::exception& e){error=e.what();}
        bool preserved=false;
        {
            study_meta::SqliteDb db(path);
            study_meta::Statement q(db.get(),"SELECT elo FROM players");
            preserved=q.next() && q.integer_column(0)==1500;
        }
        const bool ok=fired && error.find(cases[i].second)!=std::string::npos && preserved;
        std::cout<<"metadata fault "<<i<<": "<<(ok?"passed":"FAILED")<<" preserved="<<preserved<<'\n';
        failures+=!ok;
    }
    return failures?1:0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
