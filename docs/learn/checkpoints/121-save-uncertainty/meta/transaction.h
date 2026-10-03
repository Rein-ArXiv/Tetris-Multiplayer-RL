#ifndef STUDY_META_TRANSACTION_H
#define STUDY_META_TRANSACTION_H

#include "meta/sqlite_handle.h"

#include <sqlite3.h>

#include <stdexcept>

namespace study_meta {

// The borrowed SqliteDb must outlive this transaction. The rollback in the
// destructor is best effort; if storage fails there, the caller must discard
// the connection.
class ImmediateTransaction {
public:
    explicit ImmediateTransaction(SqliteDb& db) : db_(db) {
        if (sqlite3_get_autocommit(db_.get()) == 0) {
            throw std::invalid_argument("ImmediateTransaction: nested transaction rejected");
        }
        db_.exec("BEGIN IMMEDIATE");
        active_ = true;
    }

    ~ImmediateTransaction() noexcept {
        if (active_) {
            sqlite3_exec(db_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    ImmediateTransaction(const ImmediateTransaction&) = delete;
    ImmediateTransaction& operator=(const ImmediateTransaction&) = delete;
    ImmediateTransaction(ImmediateTransaction&&) = delete;
    ImmediateTransaction& operator=(ImmediateTransaction&&) = delete;

    void commit() {
        if (!active_) {
            throw std::logic_error("ImmediateTransaction: commit without active transaction");
        }
        db_.exec("COMMIT");
        active_ = false;
    }

private:
    SqliteDb& db_;
    bool active_ = false;
};

}  // namespace study_meta

#endif  // STUDY_META_TRANSACTION_H
