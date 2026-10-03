#pragma once
#include <array>
#include <cstddef>

namespace audio_pool {

// Tracks a fixed set of voice slots ordered oldest-to-newest by start time.
// order_ is always a permutation of [0, N): the front is the least recently
// started slot (the natural steal candidate) and the back is the newest.
// Pure value type: no timestamps, no heap, every operation is noexcept.
template <std::size_t N>
class VoiceOrder {
    static_assert(N > 0, "VoiceOrder requires at least one voice slot");

public:
    VoiceOrder() noexcept { reset(); }

    // Restore the identity order 0, 1, ... N-1.
    void reset() noexcept {
        for (std::size_t i = 0; i < N; ++i) order_[i] = i;
    }

    // Least recently started slot.
    std::size_t oldest() const noexcept { return order_[0]; }

    // Move slot to the newest position, removing any earlier occurrence by
    // shifting the remaining entries left. Returns false (no change) when slot
    // is out of range.
    bool mark_started(std::size_t slot) noexcept {
        if (slot >= N) return false;
        std::size_t write = 0;
        for (std::size_t read = 0; read < N; ++read) {
            if (order_[read] != slot) order_[write++] = order_[read];
        }
        order_[N - 1] = slot;
        return true;
    }

    const std::array<std::size_t, N>& ordered() const noexcept { return order_; }

private:
    std::array<std::size_t, N> order_{};
};

}  // namespace audio_pool
