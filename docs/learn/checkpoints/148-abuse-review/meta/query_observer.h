#pragma once

// Local diagnostic SELECTs: plans and statement work, outside request handling.

#include <sqlite3.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace study_meta {

// Summary of one observed query: result rows, plan lines, and work counters.
struct QueryObservation {
    std::vector<std::vector<std::optional<std::string>>> rows;
    std::vector<std::string> plan;
    int fullscan_steps = 0;
    int sorts = 0;
    int vm_steps = 0;
};

// Caller-supplied binder; receives the prepared statement (borrowed).
using QueryBind = std::function<void(sqlite3_stmt*)>;

// Finalize a prepared statement exactly once, error paths included.
struct ObserverStatementDeleter {
    void operator()(sqlite3_stmt* stmt) const noexcept {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, ObserverStatementDeleter>;

// Prepare one statement and hand ownership to RAII immediately. The raw
// pointer is guarded even when prepare fails, so nothing leaks.
inline StatementPtr observer_prepare(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* raw = nullptr;
    const int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);
    StatementPtr guard(raw);  // Guard first, check afterwards.
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("query_observer: prepare failed: ") + sqlite3_errmsg(db));
    }
    if (raw == nullptr) {
        throw std::runtime_error("query_observer: prepare produced no statement");
    }
    return guard;
}

// Copy a column as text. Caller must ensure the column is not SQLITE_NULL
// when NULL is an error; a null pointer here means NULL or out of memory.
inline std::string observer_text(sqlite3_stmt* stmt, int column) {
    const unsigned char* text = sqlite3_column_text(stmt, column);
    if (text == nullptr) {
        throw std::runtime_error("query_observer: missing text value");
    }
    const int bytes = sqlite3_column_bytes(stmt, column);
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}

// Bind a UTF-8 string; SQLite copies it (SQLITE_TRANSIENT).
inline void bind_observer_text(sqlite3_stmt* stmt, int index, const std::string& value) {
    const int rc = sqlite3_bind_text64(stmt, index, value.c_str(), static_cast<sqlite3_uint64>(value.size()), SQLITE_TRANSIENT, SQLITE_UTF8);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("query_observer: bind_text failed");
    }
}

// Bind a 64-bit integer.
inline void bind_observer_int(sqlite3_stmt* stmt, int index, sqlite3_int64 value) {
    const int rc = sqlite3_bind_int64(stmt, index, value);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("query_observer: bind_int failed");
    }
}

// Run one fixed SELECT and collect its plan, rows, and work counters.
//
// The connection is borrowed and must outlive the call. The binder receives a
// borrowed statement; it must not close or retain it. If the binder throws,
// RAII still finalizes the statement.
inline QueryObservation observe_query(sqlite3* db, const std::string& sql, const QueryBind& bind) {
    if (db == nullptr) {
        throw std::runtime_error("query_observer: null database connection");
    }
    if (sql.empty()) {
        throw std::runtime_error("query_observer: empty SQL");
    }

    QueryObservation observation;

    // Explain phase is scoped so its statement is finalized before the SELECT.
    {
        StatementPtr explain = observer_prepare(db, "EXPLAIN QUERY PLAN " + sql);
        if (bind) {
            bind(explain.get());
        }
        for (;;) {
            const int rc = sqlite3_step(explain.get());
            if (rc == SQLITE_ROW) {
                // Column 3 is the plan detail. Plan strings are recorded as-is;
                // they are never compared against exact expected text.
                observation.plan.push_back(observer_text(explain.get(), 3));
            } else if (rc == SQLITE_DONE) {
                break;
            } else {
                throw std::runtime_error(std::string("query_observer: EXPLAIN step failed: ") + sqlite3_errmsg(db));
            }
        }
    }

    StatementPtr query = observer_prepare(db, sql);
    if (bind) {
        bind(query.get());
    }
    for (;;) {
        const int rc = sqlite3_step(query.get());
        if (rc == SQLITE_ROW) {
            const int columns = sqlite3_column_count(query.get());
            std::vector<std::optional<std::string>> row;
            row.reserve(static_cast<std::size_t>(columns));
            for (int column = 0; column < columns; ++column) {
                if (sqlite3_column_type(query.get(), column) == SQLITE_NULL) {
                    row.emplace_back(std::nullopt);
                } else {
                    // Text form on purpose; numeric values are converted to text.
                    row.emplace_back(observer_text(query.get(), column));
                }
            }
            observation.rows.push_back(std::move(row));
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throw std::runtime_error(std::string("query_observer: SELECT step failed: ") + sqlite3_errmsg(db));
        }
    }

    // Statement-local work counters, not disk reads and not elapsed time.
    observation.fullscan_steps = sqlite3_stmt_status(query.get(), SQLITE_STMTSTATUS_FULLSCAN_STEP, 0);
    observation.sorts = sqlite3_stmt_status(query.get(), SQLITE_STMTSTATUS_SORT, 0);
    observation.vm_steps = sqlite3_stmt_status(query.get(), SQLITE_STMTSTATUS_VM_STEP, 0);
    return observation;
}

}  // namespace study_meta
