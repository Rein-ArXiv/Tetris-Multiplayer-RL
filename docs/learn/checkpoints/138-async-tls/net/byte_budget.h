#pragma once

#include <atomic>
#include <cstddef>

namespace study_net {

// Reserve `amount` bytes against a shared, fixed `limit`.
// Counts pending wire bytes and reservations, not RSS or kernel buffers.
// Only the counter is shared; each caller owns its storage independently.
// On success `used` grows by `amount` and `total` is the new shared count. On failure `total` is untouched. The caller must later roll back
// once (storage error) or release once (bytes accepted/discarded); this
// function performs neither.
inline bool try_reserve_bytes(std::atomic<std::size_t>& used, std::size_t limit,
                              std::size_t amount, std::size_t& total) noexcept {
    std::size_t current = used.load(std::memory_order_relaxed);
    for (;;) {
        // A zero-byte request succeeds only while the counter is within limit.
        // Bounds are checked before the subtraction to avoid underflow.
        if (current > limit || amount > limit - current) {
            return false;
        }
        // `current + amount <= limit`, so the addition cannot overflow.
        if (used.compare_exchange_weak(current, current + amount,
                                       std::memory_order_relaxed,
                                       std::memory_order_relaxed)) {
            total = current + amount;  // count committed by this reservation
            return true;
        }
        // CAS refreshed `current` with the latest value; retry.
    }
}

}  // namespace study_net
