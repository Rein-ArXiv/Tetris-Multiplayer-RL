#ifndef STUDY_META_SQLITE_HANDLE_H
#define STUDY_META_SQLITE_HANDLE_H

// Minimal RAII wrappers around an owned SQLite connection and a single-use
// prepared statement. No schema, retry, auth, or business logic lives here.

#include "sqlite3.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace study_meta {

class SqliteDb {
public:
    explicit SqliteDb(const std::string& path) {
        // An empty path or an embedded NUL would silently resolve to a
        // different identity than the caller wrote, so reject both up front.
        if (path.empty()) {
            throw std::invalid_argument("SQLite path must not be empty");
        }
        if (path.find('\0') != std::string::npos) {
            throw std::invalid_argument("SQLite path must not contain NUL");
        }

        sqlite3* raw = nullptr;
        const int rc = sqlite3_open_v2(
            path.c_str(), &raw,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);

        // Guard the handle immediately, even on failure: SQLite may return a
        // handle carrying an error message that still has to be closed.
        std::unique_ptr<sqlite3, Close> guard(raw);

        if (rc != SQLITE_OK) {
            const char* message =
                raw != nullptr ? sqlite3_errmsg(raw) : "unknown error";
            throw std::runtime_error(std::string("SQLite open failed: ") + message);
        }
        if (raw == nullptr) {
            throw std::runtime_error("SQLite open failed: no handle");
        }

        handle_ = std::move(guard);

        if (sqlite3_busy_timeout(handle_.get(), 1000) != SQLITE_OK) {
            throw std::runtime_error("SQLite busy timeout setup failed");
        }
        // Foreign keys and synchronous mode remain the caller's responsibility.
    }

    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;
    SqliteDb(SqliteDb&&) = delete;
    SqliteDb& operator=(SqliteDb&&) = delete;

    ~SqliteDb() = default;

    // Borrowed handle: the caller must not close it or outlive this object.
    sqlite3* get() const noexcept { return handle_.get(); }

    void exec(const char* sql) {
        // The error out-parameter stays null so SQLite never allocates a
        // message we would have to free; keep the generic text only.
        const int rc =
            sqlite3_exec(handle_.get(), sql, nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("SQLite exec failed");
        }
    }

private:
    struct Close {
        void operator()(sqlite3* db) const noexcept {
            if (db != nullptr) {
                sqlite3_close_v2(db);
            }
        }
    };

    std::unique_ptr<sqlite3, Close> handle_;
};

class Statement {
public:
    Statement(sqlite3* db, const char* sql) {
        if (db == nullptr) {
            throw std::invalid_argument("Statement requires an open database");
        }

        sqlite3_stmt* raw = nullptr;
        const int rc = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);

        // Guard before any throw so a partially prepared handle is finalized.
        std::unique_ptr<sqlite3_stmt, Finalize> guard(raw);

        if (rc != SQLITE_OK) {
            throw std::runtime_error("SQLite prepare failed");
        }
        if (raw == nullptr) {
            throw std::runtime_error("SQLite prepare produced no statement");
        }

        stmt_ = std::move(guard);
        // The owning database must outlive this statement.
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) = delete;
    Statement& operator=(Statement&&) = delete;

    ~Statement() = default;

    // SQLITE_TRANSIENT copies the bytes now, so a caller's local string may
    // expire as soon as this call returns. Embedded NULs are preserved here;
    // validating a value for storage is a separate concern.
    void text(int index, const std::string& value) {
        const int rc = sqlite3_bind_text64(
            stmt_.get(), index, value.data(),
            static_cast<sqlite3_uint64>(value.size()), SQLITE_TRANSIENT,
            SQLITE_UTF8);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("SQLite bind text failed");
        }
    }

    void integer(int index, sqlite3_int64 value) {
        if (sqlite3_bind_int64(stmt_.get(), index, value) != SQLITE_OK) {
            throw std::runtime_error("SQLite bind integer failed");
        }
    }

    void null(int index) {
        if (sqlite3_bind_null(stmt_.get(), index) != SQLITE_OK) {
            throw std::runtime_error("SQLite bind null failed");
        }
    }

    bool next() {
        const int rc = sqlite3_step(stmt_.get());
        if (rc == SQLITE_ROW) {
            return true;
        }
        if (rc == SQLITE_DONE) {
            return false;
        }
        throw std::runtime_error("SQLite step failed");
    }

    // For write statements: a row result is a programming error, not success.
    void done() {
        const int rc = sqlite3_step(stmt_.get());
        if (rc != SQLITE_DONE) {
            throw std::runtime_error("SQLite statement did not complete");
        }
    }

    // Reads never coerce: check the current value's storage class, not the
    // column declaration (ordinary SQLite tables have flexible typing).
    sqlite3_int64 integer_column(int column) const {
        if (sqlite3_column_type(stmt_.get(), column) != SQLITE_INTEGER) {
            throw std::runtime_error("SQLite column is not an integer");
        }
        return sqlite3_column_int64(stmt_.get(), column);
    }

    std::string text_column(int column) const {
        if (sqlite3_column_type(stmt_.get(), column) != SQLITE_TEXT) {
            throw std::runtime_error("SQLite column is not text");
        }
        const unsigned char* bytes = sqlite3_column_text(stmt_.get(), column);
        if (bytes == nullptr) {
            // Not NULL (checked above), so a null pointer means allocation
            // failure inside SQLite.
            throw std::runtime_error("SQLite text column read failed");
        }
        const int size = sqlite3_column_bytes(stmt_.get(), column);
        if (size < 0) {
            throw std::runtime_error("SQLite text column size failed");
        }
        return std::string(reinterpret_cast<const char*>(bytes),
                           static_cast<std::size_t>(size));
    }

    bool is_null(int column) const {
        return sqlite3_column_type(stmt_.get(), column) == SQLITE_NULL;
    }

private:
    struct Finalize {
        void operator()(sqlite3_stmt* stmt) const noexcept {
            if (stmt != nullptr) {
                sqlite3_finalize(stmt);
            }
        }
    };

    std::unique_ptr<sqlite3_stmt, Finalize> stmt_;
};

}  // namespace study_meta

#endif  // STUDY_META_SQLITE_HANDLE_H