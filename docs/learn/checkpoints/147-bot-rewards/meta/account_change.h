#pragma once
// Atomic credential changes over account_keys. The caller serializes calls;
// each call owns its BEGIN IMMEDIATE transaction.

#include "meta/credential_lab.h"
#include "meta/sqlite_handle.h"
#include "meta/transaction.h"

#include <charconv>
#include <cstdint>
#include <functional>
#include <optional>
#include <limits>
#include <string>
#include <string_view>

namespace study_meta {

// Recovery credential: "rc1." plus lowercase hex for exactly 32 bytes.
inline constexpr std::size_t recovery_bytes = 32;
inline constexpr std::string_view recovery_prefix = "rc1.";

inline bool valid_recovery(std::string_view value) {
    const std::size_t hex_len =
        recovery_bytes * static_cast<std::size_t>(study_credentials::hex_per_byte);
    if (value.size() != recovery_prefix.size() + hex_len) return false;
    if (value.substr(0, recovery_prefix.size()) != recovery_prefix) return false;
    for (const char c : value.substr(recovery_prefix.size())) {
        const bool lower_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!lower_hex) return false;
    }
    return true;
}

enum class ChangeKind { backup, rotate, recover };

struct ChangeRequest {
    ChangeKind kind = ChangeKind::backup;
    std::string current;
    std::string next_token;
    std::string next_recovery;
};

enum class ChangeStatus { ok, invalid_request, invalid_credential, conflict, storage_error };

struct ChangeResult {
    ChangeStatus status = ChangeStatus::storage_error;
    std::uint64_t player_id = 0;
    std::int64_t epoch = 0;
};

enum class ChangePoint { after_update, before_commit };
using ChangeHook = std::function<void(ChangePoint)>;

// Canonical positive uint64 decimal: no sign, no leading zero, full consumption.
inline bool parse_player_id(std::string_view text, std::uint64_t& out) {
    if (text.empty() || text.front() < '1' || text.front() > '9') return false;
    std::uint64_t value = 0;
    const char* const first = text.data();
    const char* const last = text.data() + text.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last) return false;
    out = value;
    return true;
}

namespace sql {
inline constexpr const char* select_retry =
    "SELECT player_id, auth_epoch FROM account_keys "
    "WHERE last_change_hash = ?1 AND token_hash = ?2 AND recovery_hash = ?3";
inline constexpr const char* select_owner_by_token =
    "SELECT player_id, auth_epoch, token_hash, recovery_hash FROM account_keys WHERE token_hash = ?1";
inline constexpr const char* select_owner_by_recovery =
    "SELECT player_id, auth_epoch, token_hash, recovery_hash FROM account_keys WHERE recovery_hash = ?1";
inline constexpr const char* update_owner =
    "UPDATE account_keys SET token_hash = ?1, recovery_hash = ?2, "
    "auth_epoch = auth_epoch + 1, last_change_hash = ?3 "
    "WHERE player_id = ?4 AND auth_epoch = ?5";
}  // namespace sql

inline ChangeResult change_credentials(SqliteDb& db, const ChangeRequest& request,
                                       const ChangeHook& hook = {}) {
    const char* kind_name = nullptr;
    study_credentials::Purpose role = study_credentials::Purpose::account;

    switch (request.kind) {
    case ChangeKind::backup:
        if (request.next_token != request.current) return ChangeResult{ChangeStatus::invalid_request};
        kind_name = "backup";
        break;
    case ChangeKind::rotate:
        if (request.next_token == request.current) return ChangeResult{ChangeStatus::invalid_request};
        kind_name = "rotate";
        break;
    case ChangeKind::recover:
        if (request.next_recovery == request.current) return ChangeResult{ChangeStatus::invalid_request};
        kind_name = "recover";
        role = study_credentials::Purpose::recovery;
        break;
    default:
        return ChangeResult{ChangeStatus::invalid_request};
    }

    const bool current_is_account = role == study_credentials::Purpose::account;
    const bool shapes_ok =
        study_credentials::valid_account(request.next_token) &&
        valid_recovery(request.next_recovery) &&
        (current_is_account ? study_credentials::valid_account(request.current)
                            : valid_recovery(request.current));
    if (!shapes_ok) return ChangeResult{ChangeStatus::invalid_request};

    try {
        // Hash before acquiring the writer transaction. Failures publish no result.
        const std::string current_hash = study_credentials::digest(role, request.current);
        const std::string next_token_hash =
            study_credentials::digest(study_credentials::Purpose::account, request.next_token);
        const std::string next_recovery_hash =
            study_credentials::digest(study_credentials::Purpose::recovery, request.next_recovery);
        const std::string receipt = study_credentials::digest(
            study_credentials::Purpose::operation,
            std::string(kind_name) + ":" + request.current + ":" + request.next_token + ":" +
                request.next_recovery);

        ImmediateTransaction tx(db);

        // Copy a possible receipt result, then close its SELECT before committing.
        std::optional<ChangeResult> replay;
        {
            Statement retry(db.get(), sql::select_retry);
            retry.text(1, receipt);
            retry.text(2, next_token_hash);
            retry.text(3, next_recovery_hash);
            if (retry.next()) {
                const auto owner = retry.text_column(0);
                const auto epoch = retry.integer_column(1);
                std::uint64_t player_id = 0;
                if (!parse_player_id(owner, player_id) || epoch < 0 || retry.next())
                    return ChangeResult{ChangeStatus::storage_error};
                replay = ChangeResult{ChangeStatus::ok, player_id, epoch};
            }
        }
        if (replay) {
            tx.commit();
            return *replay;
        }

        // Resolve the owner from the presented current credential.
        std::string owner;
        std::int64_t owner_epoch = 0;
        {
            Statement lookup(db.get(), current_is_account ? sql::select_owner_by_token
                                                          : sql::select_owner_by_recovery);
            lookup.text(1, current_hash);
            if (!lookup.next()) return ChangeResult{ChangeStatus::invalid_credential};
            owner = lookup.text_column(0);
            owner_epoch = lookup.integer_column(1);
            const auto stored_token = lookup.text_column(2);
            const auto stored_recovery = lookup.is_null(3)
                ? std::optional<std::string>{} : std::optional<std::string>{lookup.text_column(3)};
            if ((!current_is_account && stored_token == next_token_hash) ||
                (stored_recovery && *stored_recovery == next_recovery_hash))
                return ChangeResult{ChangeStatus::invalid_request};
            lookup.done();
        }

        std::uint64_t player_id = 0;
        if (!parse_player_id(owner, player_id)) return ChangeResult{ChangeStatus::storage_error};
        if (owner_epoch < 0 || owner_epoch == std::numeric_limits<std::int64_t>::max()) return ChangeResult{ChangeStatus::storage_error};

        {
            Statement update(db.get(), sql::update_owner);
            update.text(1, next_token_hash);
            update.text(2, next_recovery_hash);
            update.text(3, receipt);
            update.text(4, owner);
            update.integer(5, owner_epoch);
            try {
                update.done();
            } catch (...) {
                // Read the code before the transaction RAII clears the error.
                const int code = sqlite3_extended_errcode(db.get());
                if (code == SQLITE_CONSTRAINT_UNIQUE || code == SQLITE_CONSTRAINT_PRIMARYKEY) return ChangeResult{ChangeStatus::conflict};
                throw;
            }
            if (sqlite3_changes(db.get()) != 1) return ChangeResult{ChangeStatus::storage_error};
        }

        if (hook) hook(ChangePoint::after_update);
        const ChangeResult committed{ChangeStatus::ok, player_id, owner_epoch + 1};
        if (hook) hook(ChangePoint::before_commit);

        tx.commit();
        return committed;
    } catch (...) {
        return ChangeResult{ChangeStatus::storage_error};
    }
}

}  // namespace study_meta
