#include "meta/credential_migration.h"
#include <filesystem>
#include <iostream>
#include <limits>
using namespace study_migration;
static void require(bool v) { if (!v) throw std::runtime_error("migration contract"); }
static std::string token(char digit) { return std::string(study_credentials::account_bytes * study_credentials::hex_per_byte, digit); }
static void fixture(SqliteDb& db) {
    db.exec("CREATE TABLE players(id INTEGER PRIMARY KEY,token TEXT UNIQUE NOT NULL,bp INTEGER NOT NULL)");
    for (auto item : {std::pair<sqlite3_int64,char>{std::numeric_limits<sqlite3_int64>::max(),'a'}, {0,'b'}, {std::numeric_limits<sqlite3_int64>::min(),'c'}}) {
        Statement q(db.get(),"INSERT INTO players VALUES(?1,?2,80)");q.integer(1,item.first);q.text(2,token(item.second));q.done();
    }
}
static std::string column(SqliteDb& db, const char* sql) {
    Statement q(db.get(),sql);require(q.next());return q.text_column(0);
}
template<class F> static void fails(F f) {
    try { f(); } catch (const std::exception&) { return; }
    throw std::runtime_error("unexpected migration success");
}
int main(int argc,char** argv) {
    try {
        require(argc==2);
        const std::filesystem::path root=argv[1];
        require(!std::filesystem::exists(root));std::filesystem::create_directories(root);
        {
            SqliteDb db((root/"success.db").string());fixture(db);
            int converted=0;migrate(db,[&](Point p){if(p==Point::after_row)++converted;});require(converted==3);
            require(has_step(db,"hash_v1") && has_step(db,"scrub_v1"));
            require(column(db,"SELECT token_hash FROM players WHERE id=0")==study_credentials::account_digest(token('b')));
            Statement q(db.get(),"SELECT count(*),sum(bp) FROM players");require(q.next() && q.integer_column(0)==3 && q.integer_column(1)==240);
        }
        {
            SqliteDb db((root/"success.db").string());
            migrate(db,[](Point p){if(p==Point::after_row)throw std::runtime_error("double hash");});
        }
        for (auto stop : {Point::after_row,Point::after_commit,Point::after_vacuum,Point::before_scrub_marker}) {
            const auto path=(root/("stop-"+std::to_string(static_cast<int>(stop))+".db")).string();
            {
                SqliteDb db(path);fixture(db);
                fails([&]{migrate(db,[&](Point p){if(p==stop)throw std::runtime_error("injected");});});
                if(stop==Point::after_row)require(column(db,"SELECT token FROM players WHERE id=0")==token('b'));
                else require(has_step(db,"hash_v1") && !has_step(db,"scrub_v1"));
            }
            SqliteDb db(path);migrate(db);require(has_step(db,"scrub_v1"));
        }
        for (const auto& invalid : {std::string("bad"),token('a')+std::string("\0tail",5)}) {
            const auto path=(root/(invalid=="bad"?"invalid.db":"nul.db")).string();SqliteDb db(path);fixture(db);
            {Statement q(db.get(),"UPDATE players SET token=?1 WHERE id=0");q.text(1,invalid);q.done();}
            fails([&]{migrate(db);});
            require(column(db,"SELECT token FROM players WHERE id=0")==invalid);
            require(column(db,"SELECT token FROM players ORDER BY id LIMIT 1")==token('c'));
        }
        {
            SqliteDb db((root/"blob.db").string());fixture(db);db.exec("UPDATE players SET token=CAST(token AS BLOB) WHERE id=0");
            fails([&]{migrate(db);});require(column(db,"SELECT token FROM players ORDER BY id LIMIT 1")==token('c'));
        }
        {
            SqliteDb db((root/"nested.db").string());fixture(db);db.exec("BEGIN");fails([&]{migrate(db);});db.exec("ROLLBACK");
            db.exec("ATTACH ':memory:' AS other");fails([&]{migrate(db);});
        }
        {
            SqliteDb db((root/"bad-marker.db").string());fixture(db);
            db.exec("CREATE TABLE credential_steps(name TEXT PRIMARY KEY);INSERT INTO credential_steps VALUES('hash_v1')");
            fails([&]{migrate(db);});require(column(db,"SELECT token FROM players WHERE id=0")==token('b'));
        }
        std::cout << "atomic rows, key extremes, restart, scrub phases, invalid bytes/types and boundary contracts passed\n";
        return 0;
    } catch(const std::exception&) {std::cerr<<"migration contract failed\n";return 1;}
}
