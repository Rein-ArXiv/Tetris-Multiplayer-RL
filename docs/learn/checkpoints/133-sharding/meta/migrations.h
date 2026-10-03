#pragma once
#include "meta/schema.h"
#include "meta/history_queries.h"
#include "meta/transaction.h"
#include "meta/reward_schema.h"
#include "meta/progression_schema.h"
#include "meta/account_schema.h"
#include <cctype>
#include <map>
#include <vector>
#include <functional>
namespace study_meta {
inline constexpr int kStudyApplication = 1413829714; // ASCII TETR
inline constexpr const char* kMigrationTable = R"sql(
CREATE TABLE study_schema_history (
  version INTEGER PRIMARY KEY CHECK(version>0),
  name TEXT NOT NULL UNIQUE
);
)sql";
inline constexpr const char* kSelectionColumn =
    "ALTER TABLE players ADD COLUMN selected_icon_id TEXT NOT NULL DEFAULT 'default'";
enum class MigrationPoint { after_column, after_ownership, before_commit };
using MigrationHook = std::function<void(MigrationPoint)>;

inline sqlite3_int64 schema_integer(SqliteDb& db, const char* sql) {
    Statement q(db.get(),sql);
    if(!q.next())throw std::runtime_error("schema integer missing");
    return q.integer_column(0);
}

// A conservative signature of known declarations, not a general SQL equivalence test.
// Keep token boundaries and quoted contents while ignoring whitespace between tokens.
inline std::vector<std::string> schema_tokens(const std::string& sql) {
    std::vector<std::string> result;
    for(std::size_t i=0;i<sql.size();) {
        const auto c=static_cast<unsigned char>(sql[i]);
        if(std::isspace(c)){++i;continue;}
        const auto begin=i;
        if(sql[i]=='\'' || sql[i]=='"' || sql[i]=='`' || sql[i]=='[') {
            const char end=sql[i]=='[' ? ']' : sql[i];
            ++i;bool closed=false;
            while(i<sql.size()) {
                if(sql[i++]==end) {
                    if(i<sql.size() && sql[i]==end){++i;continue;}
                    closed=true;break;
                }
            }
            if(!closed)throw std::runtime_error("invalid schema quote");
        }else if(std::isalnum(c) || c=='_') {
            do {++i;} while(i<sql.size() &&
                (std::isalnum(static_cast<unsigned char>(sql[i])) || sql[i]=='_'));
        }else {++i;}
        result.push_back(sql.substr(begin,i-begin));
    }
    return result;
}
using TableShapes = std::map<std::string,std::vector<std::string>>;
inline TableShapes table_shapes(SqliteDb& db) {
    Statement q(db.get(),"SELECT name,sql FROM sqlite_schema WHERE type='table' "
                         "AND name NOT GLOB 'sqlite_*' ORDER BY name");
    TableShapes result;
    while(q.next())result.emplace(q.text_column(0),schema_tokens(q.text_column(1)));
    return result;
}
inline void require_shape(SqliteDb& db, int version, bool ledger) {
    SqliteDb expected(":memory:");
    expected.exec(kStudySchema);
    if(ledger)expected.exec(kMigrationTable);
    if(version>=2)expected.exec(kSelectionColumn);
    if(version>=3)expected.exec(kRewardSchema);
    if(version>=4)expected.exec(kProgressionSchema);
    if(version>=5)expected.exec(kAccountSchema);
    if(table_shapes(db)!=table_shapes(expected))
        throw std::runtime_error("unsupported study table definitions");
    Statement objects(db.get(),"SELECT 1 FROM sqlite_schema WHERE type IN ('view','trigger') LIMIT 1");
    if(objects.next())throw std::runtime_error("unsupported study view or trigger");
}
inline int recorded_version(SqliteDb& db) {
    Statement q(db.get(),"SELECT version,name FROM study_schema_history ORDER BY version");
    const char* names[]={"base_tables_v1","default_selection_v2","atomic_rewards_v3","progression_v4","guest_accounts_v5"};
    int count=0;
    while(q.next()) {
        if(count>=5 || q.integer_column(0)!=count+1 || q.text_column(1)!=names[count])
            throw std::runtime_error("unsupported study migration history");
        ++count;
    }
    if(count==0)throw std::runtime_error("empty study migration history");
    return count;
}
inline void require_data(SqliteDb& db) {
    {Statement q(db.get(),"PRAGMA quick_check");
     if(!q.next() || q.text_column(0)!="ok")throw std::runtime_error("study integrity check failed");}
    {Statement q(db.get(),"PRAGMA foreign_key_check");
     if(q.next())throw std::runtime_error("study foreign key violation");}
    Statement selection(db.get(),"SELECT 1 FROM players p WHERE NOT EXISTS "
        "(SELECT 1 FROM player_icons i WHERE i.player_id=p.id AND i.icon_id=p.selected_icon_id) LIMIT 1");
    if(selection.next())throw std::runtime_error("selected icon is not owned");
    Statement wallets(db.get(),"SELECT 1 FROM players p WHERE NOT EXISTS "
        "(SELECT 1 FROM wallets w WHERE w.player_id=p.id) LIMIT 1");
    if(wallets.next())throw std::runtime_error("player wallet missing");
    Statement rewards(db.get(),"SELECT 1 FROM matches m WHERE "
        "(SELECT count(*) FROM match_rewards r WHERE r.match_id=m.id)!=2 OR "
        "EXISTS(SELECT 1 FROM match_rewards r WHERE r.match_id=m.id "
        "AND r.player_id NOT IN (m.player_a,m.player_b)) LIMIT 1");
    if(rewards.next())throw std::runtime_error("match reward pair missing");
    Statement careers(db.get(),"SELECT 1 FROM players p WHERE NOT EXISTS "
        "(SELECT 1 FROM careers c WHERE c.player_id=p.id) LIMIT 1");
    if(careers.next())throw std::runtime_error("career missing");
    Statement progress(db.get(),"SELECT 1 FROM matches m WHERE "
        "(SELECT count(*) FROM match_progress p WHERE p.match_id=m.id)!=2 OR "
        "EXISTS(SELECT 1 FROM match_progress p WHERE p.match_id=m.id "
        "AND p.player_id NOT IN (m.player_a,m.player_b)) LIMIT 1");
    if(progress.next())throw std::runtime_error("match progress pair missing");
}

// Called before request threads start. A hook is only for local failure experiments.
inline void migrate_study(SqliteDb& db, const MigrationHook& hook={}) {
    if(schema_integer(db,"PRAGMA foreign_keys")!=1)
        throw std::invalid_argument("foreign key checks must be enabled");
    ImmediateTransaction transaction(db);
    const auto application=schema_integer(db,"PRAGMA application_id");
    const auto header_version=schema_integer(db,"PRAGMA user_version");
    if(application!=0 && application!=kStudyApplication)
        throw std::runtime_error("database belongs to another application");
    if(header_version<0 || header_version>5)
        throw std::runtime_error("unsupported study header version");
    const auto tables=table_shapes(db);
    const bool has_history=tables.count("study_schema_history")!=0;
    int version=1;
    if(!has_history) {
        if(application!=0 || header_version!=0)
            throw std::runtime_error("study history missing for tagged database");
        if(tables.empty())db.exec(kStudySchema);
        require_shape(db,1,false);
        db.exec(kMigrationTable);
        db.exec("INSERT INTO study_schema_history VALUES(1,'base_tables_v1');");
    }else {
        version=recorded_version(db);
        if(header_version!=0 && header_version!=version)
            throw std::runtime_error("study version markers disagree");
        require_shape(db,version,true);
    }
    if(version==1) {
        db.exec(kSelectionColumn);
        if(hook)hook(MigrationPoint::after_column);
        db.exec("INSERT INTO icons(id,label) VALUES('default','Default') ON CONFLICT(id) DO NOTHING;");
        db.exec("INSERT INTO player_icons(player_id,icon_id) "
                "SELECT id,'default' FROM players WHERE true "
                "ON CONFLICT(player_id,icon_id) DO NOTHING;");
        if(hook)hook(MigrationPoint::after_ownership);
        db.exec("INSERT INTO study_schema_history VALUES(2,'default_selection_v2');");
        version=2;
    }
    if(version==2) {
        db.exec(kRewardSchema);
        db.exec(kRewardBackfill);
        db.exec("INSERT INTO study_schema_history VALUES(3,'atomic_rewards_v3');");
        version=3;
    }
    if(version==3) {
        db.exec(kProgressionSchema);
        db.exec(kProgressionBackfill);
        db.exec("INSERT INTO study_schema_history VALUES(4,'progression_v4');");
        version=4;
    }
    if(version==4) {
        db.exec(kAccountSchema);
        db.exec("INSERT INTO study_schema_history VALUES(5,'guest_accounts_v5');");
    }
    require_shape(db,5,true);
    require_data(db);
    db.exec(kHistoryIndexes);
    db.exec("PRAGMA application_id=1413829714; PRAGMA user_version=5;");
    if(hook)hook(MigrationPoint::before_commit);
    transaction.commit();
}
} // namespace study_meta
