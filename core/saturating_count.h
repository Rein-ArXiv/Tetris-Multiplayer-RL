#pragma once
#include <limits>

// Contract: current is a nonnegative count. Ignore nonpositive increments.
// Compare with the remaining capacity before addition: signed overflow is UB,
// not a portable wrapping counter. Saturation loses increments at the limit.
constexpr int saturating_add_count(int current, int added) noexcept {
    if (added <= 0) return current;
    constexpr int maximum = std::numeric_limits<int>::max();
    return added > maximum - current ? maximum : current + added;
}
