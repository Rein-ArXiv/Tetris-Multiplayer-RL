#include "meta/sqlite_results.h"
#include <filesystem>
#include <iostream>
#include <functional>
using namespace study_meta;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);} while(false)
static void rejects(const std::function<void()>& action) {
    bool caught=false;
    try{action();}catch(const std::exception&){caught=true;}
    CHECK(caught);
}
static void legacy(SqliteDb& db) {
    db.exec("PRAGMA foreign_keys=ON;");db.exec(kStudySchema);
    db.exec("INSERT INTO icons VALUES('ruby','Ruby');"
            "INSERT INTO players VALUES('101','A'),('202','B');"
            "INSERT INTO player_icons VALUES('101','ruby');"
            "INSERT INTO matches(match_key,round,player_a,player_b,winner,ticks,score_a,score_b,lines_a,lines_b) "
            "VALUES('17','1','101','202','101','11','100','0',0,0);");
}
static std::string selected(SqliteDb& db) {
    Statement q(db.get(),"SELECT selected_icon_id FROM players WHERE id='101'");CHECK(q.next());return q.text_column(0);
}
static void old_rows(SqliteDb& db) {
    CHECK(schema_integer(db,"SELECT count(*) FROM players")==2);
    CHECK(schema_integer(db,"SELECT count(*) FROM matches")==1);
    Statement match(db.get(),"SELECT match_key,score_a,winner FROM matches WHERE id=1");
    CHECK(match.next() && match.text_column(0)=="17" && match.text_column(1)=="100" && match.text_column(2)=="101");
}
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);const std::string path=argv[1];CHECK(!std::filesystem::exists(path));
    for(auto point : {MigrationPoint::after_column,MigrationPoint::after_ownership,MigrationPoint::before_commit}) {
        SqliteDb db(":memory:");legacy(db);const auto before=table_shapes(db);
        rejects([&]{migrate_study(db,[&](MigrationPoint at){if(at==point)throw std::runtime_error("injected interruption");});});
        CHECK(table_shapes(db)==before);
        CHECK(schema_integer(db,"PRAGMA user_version")==0 && schema_integer(db,"PRAGMA application_id")==0);
        CHECK(schema_integer(db,"SELECT count(*) FROM player_icons")==1);old_rows(db);
        CHECK(sqlite3_get_autocommit(db.get())!=0);
        migrate_study(db);CHECK(selected(db)=="default");old_rows(db);
        CHECK(schema_integer(db,"SELECT count(*) FROM player_icons")==3);
        db.exec("UPDATE players SET selected_icon_id='ruby' WHERE id='101';");
        migrate_study(db);CHECK(selected(db)=="ruby");
        CHECK(schema_integer(db,"SELECT count(*) FROM study_schema_history")==7);
    }
    {
        SqliteDb fresh(":memory:");fresh.exec("PRAGMA foreign_keys=ON;");migrate_study(fresh);
        CHECK(recorded_version(fresh)==7);migrate_study(fresh);CHECK(recorded_version(fresh)==7);
    }
    {
        SqliteDb db(":memory:");legacy(db);db.exec(kMigrationTable);
        db.exec("INSERT INTO study_schema_history VALUES(1,'base_tables_v1'); PRAGMA user_version=1;");
        migrate_study(db);old_rows(db);CHECK(selected(db)=="default");
    }
    {
        SqliteDb db(":memory:");legacy(db);migrate_study(db);
        db.exec("INSERT INTO study_schema_history VALUES(8,'future'); PRAGMA user_version=8;");
        const auto before=table_shapes(db);rejects([&]{migrate_study(db);});CHECK(table_shapes(db)==before);
        CHECK(schema_integer(db,"PRAGMA user_version")==8);old_rows(db);
    }
    {
        SqliteDb db(":memory:");legacy(db);db.exec("PRAGMA application_id=42;");
        rejects([&]{migrate_study(db);});CHECK(schema_integer(db,"PRAGMA application_id")==42);old_rows(db);
    }
    {
        SqliteDb db(":memory:");legacy(db);db.exec("CREATE TABLE unrelated(secret TEXT);INSERT INTO unrelated VALUES('keep');");
        rejects([&]{migrate_study(db);});CHECK(schema_integer(db,"SELECT count(*) FROM unrelated")==1);
    }
    {
        // LIKE's underscore wildcard would hide this ordinary user table.
        SqliteDb db(":memory:");db.exec("PRAGMA foreign_keys=ON;");
        db.exec("CREATE TABLE sqliteXforeign(note TEXT); INSERT INTO sqliteXforeign VALUES('keep');");
        const auto before=table_shapes(db);
        CHECK(before.count("sqliteXforeign")==1);
        rejects([&]{migrate_study(db);});
        CHECK(table_shapes(db)==before);
        CHECK(schema_integer(db,"SELECT count(*) FROM sqliteXforeign")==1);
    }
    {
        SqliteDb db(":memory:");legacy(db);db.exec(kMigrationTable);
        db.exec("INSERT INTO study_schema_history VALUES(1,'base_tables_v1'),(2,'default_selection_v2');");
        rejects([&]{migrate_study(db);});require_shape(db,1,true);
    }
    {
        SqliteDb db(":memory:");legacy(db);db.exec("PRAGMA foreign_keys=OFF;INSERT INTO player_icons VALUES('999','ruby');PRAGMA foreign_keys=ON;");
        const auto before=table_shapes(db);rejects([&]{migrate_study(db);});CHECK(table_shapes(db)==before);
    }
    {
        SqliteDb db(":memory:");db.exec("PRAGMA foreign_keys=ON;");
        ImmediateTransaction outer(db);rejects([&]{ImmediateTransaction inner(db);});
        CHECK(sqlite3_get_autocommit(db.get())==0);outer.commit();rejects([&]{outer.commit();});
    }
    {
        SqliteDb db(path);legacy(db);
        // An existing reader lets writes start but prevents rollback-journal COMMIT.
        SqliteDb reader(path);reader.exec("BEGIN;");
        Statement read(reader.get(),"SELECT display_name FROM players");CHECK(read.next());
        CHECK(sqlite3_busy_timeout(db.get(),10)==SQLITE_OK);
        bool reached_commit=false;
        rejects([&]{migrate_study(db,[&](MigrationPoint p){if(p==MigrationPoint::before_commit)reached_commit=true;});});
        CHECK(reached_commit && sqlite3_get_autocommit(db.get())!=0);
        require_shape(db,1,false);old_rows(db);
    }
    {
        SqliteResults upgraded(path);CHECK(upgraded.selected_icon(101)=="default");
        CHECK(upgraded.select_icon(101,"ruby"));CHECK(!upgraded.select_icon(202,"ruby"));
        upgraded.add_player(303,"C");CHECK(upgraded.selected_icon(303)=="default");
        CHECK((upgraded.icons(303)==std::vector<std::string>{"default"}));
        CHECK(!upgraded.selected_icon(999));
    }
    {SqliteResults again(path);CHECK(again.selected_icon(101)=="ruby");CHECK(again.lookup(17)->row==1);}
    std::cout<<"new/legacy/v1/reopen/future/foreign/mismatch/interrupt/commit-busy and selected icon preservation: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
