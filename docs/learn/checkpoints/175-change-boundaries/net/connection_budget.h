// net/connection_budget.h
//
// ConnectionBudget<MaxKeys> -- caps simultaneous occupancy of sessions,
// in-flight handshakes, and caller-declared reserved bytes for connections
// identified by integer keys.
//
// Design goals:
//   * One heap-allocated State owns every counter, guarded by a single mutex.
//     No I/O, timers, condition-variable waits, or authentication happen inside this header.
//   * Admission is O(MaxKeys) over fixed key slots and performs no heap
//     allocation per admit: State is created once, in the constructor, and
//     each admitted lease simply shares it.
//   * Keys are uint64_t normalized-source identifiers chosen by the caller.
//     They are NOT authentication tokens: this header never hashes, verifies,
//     or compares them beyond equality. If you want per-IP keys, map the
//     address to a key yourself before calling admit().
//   * A lease keeps State alive after the owning ConnectionBudget is gone, so
//     issued leases can always finish cleanup against valid memory.
//
// Limits are simultaneous occupancy caps, not rates. Nothing here implies a
// time-based or sliding-window rate limit: a few long-lived sessions can
// exhaust the budget as easily as a burst of short ones. Reserved bytes are a
// declared bound on storage owned by the connection -- not total process RSS
// and not kernel/network memory. Even with all capacities zero, the State
// object itself is still allocated at construction.

#ifndef STUDY_NET_CONNECTION_BUDGET_H
#define STUDY_NET_CONNECTION_BUDGET_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace study_net {

template <std::size_t MaxKeys>
class ConnectionBudget {
    static_assert(MaxKeys > 0, "MaxKeys must be positive");

public:
    // Six independent occupancy limits. A limit of 0 denies that resource
    // entirely (all-zero Limits denies everything). There is no assumption that
    // handshakes <= sessions.
    struct Limits {
        std::size_t total_sessions = 0;
        std::size_t per_key_sessions = 0;
        std::size_t total_handshakes = 0;
        std::size_t per_key_handshakes = 0;
        std::size_t total_bytes = 0;
        std::size_t per_connection_bytes = 0;
    };

    // Why an admission attempt ended the way it did. The rejected variants name
    // the specific cap that was hit.
    enum class AdmissionStatus {
        admitted,
        stopped,
        total_sessions,
        source_sessions,
        total_handshakes,
        source_handshakes,
        key_capacity,
    };

private:
    static constexpr std::size_t no_slot = static_cast<std::size_t>(-1);

    // A fixed key slot. sessions == 0 means the slot is free.
    struct Bucket {
        std::uint64_t key = 0;
        std::size_t sessions = 0;
        std::size_t handshakes = 0;
    };

    struct State {
        explicit State(const Limits& l) : limits(l) {}

        const Limits limits;
        std::mutex mutex;
        bool accepting = true;
        std::size_t sessions = 0;
        std::size_t handshakes = 0;
        std::size_t bytes = 0;
        std::array<Bucket, MaxKeys> buckets{};
    };

public:
    // Move-only handle for one admitted connection. Each Lease must have a
    // single serialized owner; distinct leases may be used concurrently. An
    // empty lease (the default state) is falsy and ignores all operations.
    class Lease {
        friend class ConnectionBudget;

    public:
        Lease() noexcept = default;

        ~Lease() { reset(); }

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        Lease(Lease&& other) noexcept
            : state_(std::move(other.state_)),
              slot_(other.slot_),
              handshaking_(other.handshaking_),
              held_bytes_(other.held_bytes_) {
            other.slot_ = 0;
            other.handshaking_ = false;
            other.held_bytes_ = 0;
        }

        Lease& operator=(Lease&& other) noexcept {
            if (this != &other) {
                reset();
                state_ = std::move(other.state_);
                slot_ = other.slot_;
                handshaking_ = other.handshaking_;
                held_bytes_ = other.held_bytes_;
                other.slot_ = 0;
                other.handshaking_ = false;
                other.held_bytes_ = 0;
            }
            return *this;
        }

        // Truthy while this handle owns a session.
        explicit operator bool() const noexcept {
            return static_cast<bool>(state_);
        }

        // Bytes this lease currently holds reserved.
        std::size_t bytes() const noexcept { return held_bytes_; }

        // Marks the caller's handshake phase finished. False for an empty lease
        // or if the phase already finished. This only records that the caller's
        // phase is over; it asserts nothing about authentication.
        bool finish_handshake() {
            if (!state_ || !handshaking_) return false;
            std::shared_ptr<State> state = state_;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                --state->handshakes;
                --state->buckets[slot_].handshakes;
                handshaking_ = false;
            }
            return true;
        }

        // Reserves n bytes for this connection. False for an empty lease or if
        // either the per-connection cap or the global byte cap would be passed.
        // n == 0 succeeds for any live lease and never allocates.
        bool reserve_bytes(std::size_t n) {
            if (!state_) return false;
            std::shared_ptr<State> state = state_;
            std::lock_guard<std::mutex> lock(state->mutex);
            // Subtract first: held_bytes_ <= per_connection_bytes and
            // state->bytes <= total_bytes are invariants, so neither wraps.
            if (n > state->limits.per_connection_bytes - held_bytes_) {
                return false;
            }
            if (n > state->limits.total_bytes - state->bytes) {
                return false;
            }
            held_bytes_ += n;
            state->bytes += n;
            return true;
        }

        // Releases n previously reserved bytes. False for an empty lease or if n
        // exceeds what this lease holds.
        bool release_bytes(std::size_t n) {
            if (!state_ || n > held_bytes_) return false;
            std::shared_ptr<State> state = state_;
            std::lock_guard<std::mutex> lock(state->mutex);
            held_bytes_ -= n;
            state->bytes -= n;
            return true;
        }

        // Idempotent teardown; also called by the destructor.
        void reset() noexcept {
            // Take the owning reference into a local first so State cannot
            // vanish underneath us while we unwind it.
            std::shared_ptr<State> state = std::move(state_);
            if (!state) return;

            const std::size_t slot = slot_;
            const bool handshaking = handshaking_;
            const std::size_t held = held_bytes_;
            slot_ = 0;
            handshaking_ = false;
            held_bytes_ = 0;

            {
                std::lock_guard<std::mutex> lock(state->mutex);
                --state->sessions;
                if (handshaking) --state->handshakes;
                Bucket& bucket = state->buckets[slot];
                --bucket.sessions;
                if (handshaking) --bucket.handshakes;
                if (bucket.sessions == 0) bucket = Bucket{};
                state->bytes -= held;
            }
            // The mutex is released before `state` is destroyed: this may be the
            // last reference, and destroying a locked mutex is undefined.
        }

    private:
        Lease(std::shared_ptr<State> state, std::size_t slot,
              bool handshaking) noexcept
            : state_(std::move(state)),
              slot_(slot),
              handshaking_(handshaking) {}

        std::shared_ptr<State> state_;
        std::size_t slot_ = 0;
        bool handshaking_ = false;
        std::size_t held_bytes_ = 0;
    };

    struct Admission {
        AdmissionStatus status = AdmissionStatus::stopped;
        Lease lease;
    };

    struct Snapshot {
        std::size_t sessions = 0;
        std::size_t handshakes = 0;
        std::size_t bytes = 0;
        std::size_t keys = 0;  // occupied key slots
    };

    explicit ConnectionBudget(const Limits& limits)
        : state_(std::make_shared<State>(limits)) {}

    // The owner may be destroyed once all concurrent method users have stopped;
    // outstanding leases keep State alive so their cleanup still runs.
    ~ConnectionBudget() = default;

    ConnectionBudget(const ConnectionBudget&) = delete;
    ConnectionBudget& operator=(const ConnectionBudget&) = delete;
    ConnectionBudget(ConnectionBudget&&) = delete;
    ConnectionBudget& operator=(ConnectionBudget&&) = delete;

    // Attempts to admit one connection under `key`. On success the returned
    // lease owns a session and an in-flight handshake; call finish_handshake()
    // when the caller's handshake phase ends, and reset()/destruct when the
    // connection closes.
    //
    // Rejection order is deterministic:
    //   1. stopped           -- stop_accepting() was called
    //   2. total_sessions    -- global session cap reached
    //   3. total_handshakes  -- global handshake cap reached
    //   4. source_sessions   -- this key's session cap reached / is zero
    //   5. source_handshakes -- this key's handshake cap reached / is zero
    //   6. key_capacity      -- no free slot for a new key
    // A rejected call never modifies any counter.
    Admission admit(std::uint64_t key) {
        Admission result;
        std::shared_ptr<State> state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);

        if (!state->accepting) {
            result.status = AdmissionStatus::stopped;
            return result;
        }
        if (state->sessions >= state->limits.total_sessions) {
            result.status = AdmissionStatus::total_sessions;
            return result;
        }
        if (state->handshakes >= state->limits.total_handshakes) {
            result.status = AdmissionStatus::total_handshakes;
            return result;
        }

        std::size_t slot = no_slot;
        for (std::size_t i = 0; i < MaxKeys; ++i) {
            if (state->buckets[i].sessions > 0 &&
                state->buckets[i].key == key) {
                slot = i;
                break;
            }
        }
        const bool is_new_key = (slot == no_slot);

        if (is_new_key) {
            // Zero per-key caps deny every new source, matching the all-zero
            // Limits convention. No assumption that handshakes <= sessions.
            if (state->limits.per_key_sessions == 0) {
                result.status = AdmissionStatus::source_sessions;
                return result;
            }
            if (state->limits.per_key_handshakes == 0) {
                result.status = AdmissionStatus::source_handshakes;
                return result;
            }
            for (std::size_t i = 0; i < MaxKeys; ++i) {
                if (state->buckets[i].sessions == 0) {
                    slot = i;
                    break;
                }
            }
            if (slot == no_slot) {
                result.status = AdmissionStatus::key_capacity;
                return result;
            }
        } else {
            if (state->buckets[slot].sessions >=
                state->limits.per_key_sessions) {
                result.status = AdmissionStatus::source_sessions;
                return result;
            }
            if (state->buckets[slot].handshakes >=
                state->limits.per_key_handshakes) {
                result.status = AdmissionStatus::source_handshakes;
                return result;
            }
        }

        // Commit: all checks passed. Only now do counters change, and the lease
        // reuses the State allocated at construction, so this path does not
        // allocate and the lease constructor is noexcept.
        state->sessions += 1;
        state->handshakes += 1;
        if (is_new_key) {
            state->buckets[slot].key = key;
            state->buckets[slot].sessions = 1;
            state->buckets[slot].handshakes = 1;
        } else {
            state->buckets[slot].sessions += 1;
            state->buckets[slot].handshakes += 1;
        }

        result.lease = Lease(state, slot, true);
        result.status = AdmissionStatus::admitted;
        return result;
    }

    // Consistent point-in-time view, including the number of occupied keys.
    Snapshot snapshot() const {
        std::shared_ptr<State> state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        Snapshot snap;
        snap.sessions = state->sessions;
        snap.handshakes = state->handshakes;
        snap.bytes = state->bytes;
        for (const Bucket& bucket : state->buckets) {
            if (bucket.sessions > 0) snap.keys += 1;
        }
        return snap;
    }

    // Stops admitting new connections. Existing leases are untouched; there is
    // no drain or wait.
    void stop_accepting() noexcept {
        std::shared_ptr<State> state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        state->accepting = false;
    }

private:
    std::shared_ptr<State> state_;
};

}  // namespace study_net

#endif  // STUDY_NET_CONNECTION_BUDGET_H
