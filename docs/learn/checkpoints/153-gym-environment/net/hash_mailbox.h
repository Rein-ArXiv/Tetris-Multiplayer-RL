#ifndef STUDY_NET_HASH_MAILBOX_H
#define STUDY_NET_HASH_MAILBOX_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>

namespace study_net {

struct HashSample {
    uint32_t tick;
    uint64_t hash;
};

struct HashComparison {
    uint32_t tick;
    uint64_t local;
    uint64_t remote;
};

enum class HashPut {
    stored,
    duplicate,
    stale,
    invalid,
    conflict,
    too_far,
    exhausted,
};

// Concurrent observation of one (tick, hash) pair per side.
// A mutex groups both arrays so a comparison pair is copied atomically;
// Separately atomic tick and hash fields can still be read from different
// publications, even with seq_cst. A sequence counter alone cannot legalize
// concurrent reads/writes of a non-atomic payload in C++. Only bounded,
// periodic events are compared; the overwriting latest state is separate,
// and a full window returns too_far rather than evicting anything.
template <uint32_t Period, size_t Capacity>
class HashMailbox {
    static_assert(Period > 0, "Period must be positive");
    static_assert(Capacity > 0, "Capacity must be positive");

public:
    static constexpr uint32_t period = Period;
    static constexpr size_t capacity = Capacity;

    HashMailbox() = default;

    HashMailbox(const HashMailbox&) = delete;
    HashMailbox& operator=(const HashMailbox&) = delete;
    HashMailbox(HashMailbox&&) = delete;
    HashMailbox& operator=(HashMailbox&&) = delete;

    // Record a locally produced sample for tick t.
    HashPut record_local(uint32_t t, uint64_t hash) {
        std::lock_guard<std::mutex> lock(mutex_);
        return put(local_, t, hash);
    }

    // Record a remotely received sample; on acceptance it also becomes the
    // last accepted arrival reported by latest_remote (not the max tick).
    HashPut record_remote(uint32_t t, uint64_t hash) {
        std::lock_guard<std::mutex> lock(mutex_);
        const HashPut r = put(remote_, t, hash);
        if (r == HashPut::stored || r == HashPut::duplicate) {
            latest_ = HashSample{t, hash};
        }
        return r;
    }

    // Nondestructive read of the last accepted remote arrival.
    bool latest_remote(HashSample& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!latest_) {
            return false;
        }
        out = *latest_;
        return true;
    }

    // Consume the earliest complete tick pair. A ready higher tick waits
    // behind the earliest tick even if a lower remote is missing; nothing is
    // skipped. The caller compares local/remote; no I/O under the lock.
    bool poll(HashComparison& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!local_[0] || !remote_[0]) {
            return false;
        }
        const HashComparison candidate{
            static_cast<uint32_t>(next_),
            *local_[0],
            *remote_[0],
        };
        shift(local_);
        shift(remote_);
        next_ += Period;
        out = candidate;
        return true;
    }

    // Drop all retained samples. This resets storage and the admission
    // cursor; it does not cancel messages already outside the object or
    // change any round/epoch identity. Callers must coordinate lifecycle;
    // stop or coordinate producers before reusing the cleared round state.
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        local_.fill(std::nullopt);
        remote_.fill(std::nullopt);
        latest_.reset();
        next_ = Period;
    }

    // Diagnostic peek at the earliest unconsumed tick, not a reservation.
    uint64_t next_tick() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return next_;
    }

private:
    HashPut put(std::array<std::optional<uint64_t>, Capacity>& side, uint32_t t,
                uint64_t hash) {
        if (t == 0 || t % Period != 0) {
            return HashPut::invalid;
        }
        if (next_ > static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)())) {
            return HashPut::exhausted;
        }
        if (t < next_) {
            return HashPut::stale;
        }
        const uint64_t offset = (static_cast<uint64_t>(t) - next_) / Period;
        if (offset >= Capacity) {
            return HashPut::too_far;
        }
        std::optional<uint64_t>& slot = side[static_cast<size_t>(offset)];
        if (slot) {
            return *slot == hash ? HashPut::duplicate : HashPut::conflict;
        }
        slot = hash;
        return HashPut::stored;
    }

    static void shift(std::array<std::optional<uint64_t>, Capacity>& side) {
        for (size_t i = 0; i + 1 < Capacity; ++i) {
            side[i] = side[i + 1];
        }
        side[Capacity - 1].reset();
    }

    mutable std::mutex mutex_;
    std::array<std::optional<uint64_t>, Capacity> local_{};
    std::array<std::optional<uint64_t>, Capacity> remote_{};
    std::optional<HashSample> latest_;
    uint64_t next_ = Period;
};

}  // namespace study_net

#endif  // STUDY_NET_HASH_MAILBOX_H
