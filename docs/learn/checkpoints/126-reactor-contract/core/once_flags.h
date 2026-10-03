#pragma once

// core/once_flags.h - fixed-size "first time only" flags.
// No heap, no counters, no ownership. Each slot can be taken exactly once
// until reset(); an out-of-range index is refused rather than clamped, so a
// caller bug cannot silently alias a valid slot.

#include <array>
#include <cstddef>

namespace once_flags {

template <std::size_t N>
class Flags {
public:
    // Returns true the first time a valid index is taken and false for an
    // out-of-range index or a slot that was already taken. Taking is the only
    // mutation besides reset(), so a single owning thread may call this
    // without atomics; it is not a cross-thread synchronization primitive.
    bool take(std::size_t index) noexcept {
        if (index >= N) return false;
        if (taken_[index]) return false;
        taken_[index] = true;
        return true;
    }

    // Clear every slot so each may be taken once again. This does not touch
    // any external state the flags were merely used to guard.
    void reset() noexcept { taken_.fill(false); }

private:
    std::array<bool, N> taken_{};
};

}  // namespace once_flags
