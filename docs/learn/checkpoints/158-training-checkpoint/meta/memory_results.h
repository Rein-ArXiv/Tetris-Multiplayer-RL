#ifndef STUDY_META_MEMORY_RESULTS_H
#define STUDY_META_MEMORY_RESULTS_H

#include "net/match_submission.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace study_meta {

// Storage boundary for a small, loopback-only HTTP service.
//
// This store is deliberately VOLATILE: there is no database and no durable
// medium behind it. Everything lives in the process, inside a fixed-size
// std::array, and disappears when the process exits. It is a teaching
// implementation of the "storage" seam, not a persistence layer.
//
enum class PutStatus {
    accepted,  // stored, or an identical record was already stored
    conflict,  // same key, different payload: rejected, no mutation
    full,      // new key but no free row: rejected, no mutation
    invalid    // failed basic validation: rejected, no mutation
};

struct PutResult {
    PutStatus status = PutStatus::invalid;
    study_net::Receipt receipt{};
};

// Fixed-capacity, mutex-guarded match-result store.
//
// Capacity must be positive. Rows are never evicted or reordered; once a row
// is used it stays used for the lifetime of the store.
//
// Lifetime: the store must outlive every handler that borrows it. No method
// returns a pointer, reference, or view into internal storage.
//
template <std::size_t Capacity>
class MemoryResults {
    static_assert(Capacity > 0, "MemoryResults needs at least one row");

public:
    MemoryResults() = default;

    // A store is a single, addressable owner of its rows and its mutex:
    // copying or moving it would duplicate or relocate the lock, so both are
    // deleted.
    MemoryResults(const MemoryResults&) = delete;
    MemoryResults& operator=(const MemoryResults&) = delete;
    MemoryResults(MemoryResults&&) = delete;
    MemoryResults& operator=(MemoryResults&&) = delete;

    // Insert or idempotently re-report a record.
    //
    // Thread safety: HTTP worker threads may enter put() concurrently. The key
    // lookup, payload comparison, insertion, and count update all happen inside
    // one critical section, so no two workers can observe or create a
    // half-updated row. This method is intentionally NOT noexcept because
    // locking the mutex may throw, and that exception is propagated to the
    // caller rather than swallowed.
    PutResult put(const study_net::MatchRecord& record) {
        // Basic, allocation-free validity check using the same validator the
        // network layer uses. An invalid record is rejected before we touch the
        // store, so its state is unchanged.
        study_net::MatchSubmission validator(0);
        if (!validator.prepare(record)) {
            return PutResult{PutStatus::invalid, study_net::Receipt{}};
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Existing-key check FIRST. This is what lets an identical retry still
        // succeed when the store is already full: we find the key and confirm
        // the payload matches before ever consulting Capacity.
        for (std::size_t i = 0; i < count_; ++i) {
            const std::optional<study_net::MatchRecord>& slot = rows_[i];
            if (slot.has_value() && slot->key == record.key) {
                if (*slot == record) {
                    // Idempotent accept: same key, same payload. Row is reported
                    // as a positive, one-based index.
                    return PutResult{
                        PutStatus::accepted,
                        study_net::Receipt{record.key, i + 1, record.player_a, record.player_b}};
                }
                // Same key, different payload. Never overwrite: reject.
                return PutResult{PutStatus::conflict, study_net::Receipt{}};
            }
        }

        // New key. Only now does capacity matter.
        if (count_ == Capacity) {
            return PutResult{PutStatus::full, study_net::Receipt{}};
        }

        // Copy the record into the next free row without any allocation.
        rows_[count_] = record;
        ++count_;
        // count_ is now the one-based row of the record we just stored.
        return PutResult{
            PutStatus::accepted,
            study_net::Receipt{record.key, count_, record.player_a, record.player_b}};
    }

    // Number of occupied rows. Locks a mutable mutex because the logical state
    // is read-only but the lock itself must be mutable in const methods.
    std::size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    // Return a COPY of the matching record, or nullopt.
    //
    // The lock is released before the caller uses the value. Handing out a copy
    // (rather than a pointer, reference, or view) means the returned record can
    // safely outlive the critical section; nothing outside the lock can alias
    // internal storage.
    std::optional<study_net::MatchRecord> lookup(std::uint64_t key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::size_t i = 0; i < count_; ++i) {
            const std::optional<study_net::MatchRecord>& slot = rows_[i];
            if (slot.has_value() && slot->key == key) {
                return *slot;  // copy taken under the lock
            }
        }
        return std::nullopt;
    }

private:
    // Fixed, inline storage. std::optional models an unused row without
    // allocating, and no method ever exposes this array (or the mutex) to
    // callers.
    std::array<std::optional<study_net::MatchRecord>, Capacity> rows_{};
    std::size_t count_ = 0;
    mutable std::mutex mutex_;
};

}  // namespace study_meta

#endif  // STUDY_META_MEMORY_RESULTS_H
