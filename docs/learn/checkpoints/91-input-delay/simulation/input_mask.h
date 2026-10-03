#pragma once

#include <cstdint>
#include <optional>

#include "simulation/intent.h"

namespace study_input {

// Per-tick input requests packed into a single byte mask.
using Mask = std::uint8_t;

inline constexpr Mask left = 1u << 0;
inline constexpr Mask right = 1u << 1;
inline constexpr Mask down = 1u << 2;
inline constexpr Mask rotate = 1u << 3;
inline constexpr Mask drop = 1u << 4;

inline constexpr Mask known = left | right | down | rotate | drop;

inline constexpr bool has_any(Mask value, Mask bits) noexcept {
    return (value & bits) != 0;
}

inline constexpr bool has_all(Mask value, Mask bits) noexcept {
    return (value & bits) == bits;
}

// Reject any bit outside known *before* narrowing to Mask.
inline constexpr bool valid(unsigned value) noexcept {
    return (value & ~static_cast<unsigned>(known)) == 0u;
}

// Encode an intent, accepting only horizontal in {-1, 0, +1}.
inline std::optional<Mask> encode(const Intent& intent) noexcept {
    if (intent.horizontal < -1 || intent.horizontal > 1) {
        return std::nullopt;
    }
    Mask mask = 0;
    if (intent.horizontal < 0) {
        mask |= left;
    }
    if (intent.horizontal > 0) {
        mask |= right;
    }
    if (intent.clockwise) {
        mask |= rotate;
    }
    if (intent.soft_drop) {
        mask |= down;
    }
    if (intent.hard_drop) {
        mask |= drop;
    }
    return mask;
}

// Decode a raw value after validating it. A raw left+right pair is valid as a
// raw mask but semantically neutral in this teaching policy, so it normalizes to horizontal 0.
inline std::optional<Intent> decode(unsigned value) noexcept {
    if (!valid(value)) {
        return std::nullopt;
    }
    const Mask mask = static_cast<Mask>(value);
    int horizontal = 0;
    if (has_any(mask, left) && !has_any(mask, right)) {
        horizontal = -1;
    } else if (has_any(mask, right) && !has_any(mask, left)) {
        horizontal = +1;
    }
    Intent intent{horizontal, has_any(mask, rotate)};
    intent.soft_drop = has_any(mask, down);
    intent.hard_drop = has_any(mask, drop);
    return intent;
}

} // namespace study_input
