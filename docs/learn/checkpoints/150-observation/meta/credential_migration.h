#ifndef STUDY_META_CREDENTIAL_MIGRATION_H
#define STUDY_META_CREDENTIAL_MIGRATION_H

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

#include "meta/credential_lab.h"
#include "meta/sqlite_handle.h"
#include "meta/transaction.h"

namespace study_migration {

using study_meta::ImmediateTransaction;
using study_meta::SqliteDb;
using study_meta::Statement;

// Local-only failure-injection points for experiments.
enum class Point {
    after_row,
    after_commit,
    after_vacuum,
    before_scrub_marker,
};

using Hook = std::function<void(Point)>;

// Force and verify the durability posture required before any transaction.
inline void configure(SqliteDb& db) {
    if (sqlite3_get_autocommit(db.get()) == 0)
        throw std::invalid_argument("migration requires its own transaction boundary");
    {
        Statement list(db.get(), "PRAGMA database_list");
        while (list.next()) {
            const auto name = list.text_column(1);
            if (name != "main" && name != "temp")
                throw std::invalid_argument("attached database not supported");
        }
    }
    db.exec("PRAGMA secure_delete=ON;");
    db.exec("PRAGMA synchronous=FULL;");
    db.exec("PRAGMA journal_mode=WAL;");

    {
        Statement st(db.get(), "PRAGMA journal_mode;");
        if (!st.next()) throw std::runtime_error("credential migration: journal_mode missing");
        const std::string mode = st.text_column(0);
        if (mode != "wal") throw std::runtime_error("credential migration: WAL required");
    }
    {
        Statement st(db.get(), "PRAGMA synchronous;");
        if (!st.next()) throw std::runtime_error("credential migration: synchronous missing");
        if (st.integer_column(0) != 2) throw std::runtime_error("credential migration: FULL required");
    }
    {
        Statement st(db.get(), "PRAGMA secure_delete;");
        if (!st.next()) throw std::runtime_error("credential migration: secure_delete missing");
        if (st.integer_column(0) != 1) throw std::runtime_error("credential migration: secure_delete required");
    }
}

// Reads the exact marker name; a read error propagates as failure, never absence.
inline bool has_step(SqliteDb& db, const std::string& name) {
    Statement st(db.get(), "SELECT 1 FROM credential_steps WHERE name=?1 LIMIT 1;");
    st.text(1, name);
    return st.next();
}

// TRUNCATE checkpoint; requires busy==0 and no extra row.
inline void checkpoint(SqliteDb& db) {
    Statement st(db.get(), "PRAGMA wal_checkpoint(TRUNCATE);");
    if (!st.next()) throw std::runtime_error("credential migration: checkpoint missing row");
    if (st.integer_column(0) != 0) throw std::runtime_error("credential migration: checkpoint busy");
    if (st.next()) throw std::runtime_error("credential migration: checkpoint extra row");
}

namespace detail {

struct PlayersSchema {
    bool legacy = false;  // token present
    bool hashed = false;  // token_hash present
};

// Trusted fixture schema only: exactly id, bp and one token column.
inline PlayersSchema inspect_players(SqliteDb& db) {
    Statement st(db.get(), "PRAGMA table_info(players);");
    bool id = false;
    bool bp = false;
    bool token = false;
    bool token_hash = false;
    int columns = 0;
    while (st.next()) {
        ++columns;
        const std::string name = st.text_column(1);
        if (name == "id") {
            id = true;
            if (st.text_column(2) != "INTEGER" || st.integer_column(5) != 1)
                throw std::runtime_error("credential migration: stable integer key required");
        }
        else if (name == "bp") bp = true;
        else if (name == "token") token = true;
        else if (name == "token_hash") token_hash = true;
        else throw std::runtime_error("credential migration: unexpected players column");
    }
    if (!id || !bp) throw std::runtime_error("credential migration: players missing id/bp");
    if (columns != 3) throw std::runtime_error("credential migration: unexpected players shape");
    if (token == token_hash) throw std::runtime_error("credential migration: ambiguous token column");
    PlayersSchema s;
    s.legacy = token;
    s.hashed = token_hash;
    return s;
}

inline void reject_unknown_markers(SqliteDb& db) {
    Statement st(db.get(), "SELECT name FROM credential_steps;");
    while (st.next()) {
        const std::string name = st.text_column(0);
        if (name != "hash_v1" && name != "scrub_v1")
            throw std::runtime_error("credential migration: unknown marker");
    }
}

inline void insert_marker(SqliteDb& db, const char* name) {
    Statement st(db.get(), "INSERT INTO credential_steps(name) VALUES(?1);");
    st.text(1, std::string(name));
    st.done();
}

}  // namespace detail

// Migrate the dedicated local legacy experiment DB before accepting traffic.
inline void migrate(SqliteDb& db, const Hook& hook = {}) {
    configure(db);

    bool scrub_marker = false;

    {
        ImmediateTransaction tx(db);
        db.exec("CREATE TABLE IF NOT EXISTS credential_steps(name TEXT PRIMARY KEY);");
        detail::reject_unknown_markers(db);

        const detail::PlayersSchema schema = detail::inspect_players(db);
        const bool hash_marker = has_step(db, "hash_v1");
        scrub_marker = has_step(db, "scrub_v1");

        if (schema.legacy && (hash_marker || scrub_marker))
            throw std::runtime_error("credential migration: marker on legacy players");
        if (schema.hashed && !hash_marker)
            throw std::runtime_error("credential migration: hashed players without hash marker");

        if (schema.legacy) {
            db.exec("ALTER TABLE players RENAME COLUMN token TO token_hash;");

            std::optional<sqlite3_int64> last;
            for (;;) {
                sqlite3_int64 id = 0;
                std::string token;
                {
                    const char* sql = last
                        ? "SELECT id, token_hash FROM players WHERE id>?1 ORDER BY id LIMIT 1;"
                        : "SELECT id, token_hash FROM players ORDER BY id LIMIT 1;";
                    Statement row(db.get(), sql);
                    if (last) row.integer(1, *last);
                    if (!row.next()) break;
                    id = row.integer_column(0);
                    token = row.text_column(1);
                } // Finalize the reader before changing the table.

                const std::string hash = study_credentials::account_digest(token);

                {
                    Statement up(db.get(),
                        "UPDATE players SET token_hash=?1 WHERE id=?2;");
                    up.text(1, hash);
                    up.integer(2, id);
                    up.done();
                }
                if (sqlite3_changes(db.get()) != 1)
                    throw std::runtime_error("credential migration: update count");

                last = id;
                if (hook) hook(Point::after_row);
            }

            detail::insert_marker(db, "hash_v1");
        }

        tx.commit();
    }

    if (hook) hook(Point::after_commit);

    if (!scrub_marker) {
        checkpoint(db);
        db.exec("VACUUM;");
        if (hook) hook(Point::after_vacuum);
        checkpoint(db);
        if (hook) hook(Point::before_scrub_marker);

        ImmediateTransaction tx(db);
        detail::insert_marker(db, "scrub_v1");
        tx.commit();
    }
}

}  // namespace study_migration

#endif  // STUDY_META_CREDENTIAL_MIGRATION_H
