#pragma once
// study_history: bounded tutorial helper (exercise only).
// The current src game has NO combo/streak state; this header is an added exercise,
// integrated by Round::finish_lock. No B2B, Mini, or SRS rules.

#include <cstdint>
#include <optional>

#include "simulation/score.h"  // saturating_add

namespace study_history {

struct Chain {
    // Number of consecutive LOCKS that cleared at least one line.
    std::uint64_t clears = 0;
};

struct ClearReward {
    Chain chain;   // streak AFTER this lock
    int attack = 0;
};

namespace detail {

// Combo bonus derived from the AFTER streak.
// 0/1/2 -> 0, 3/4 -> 1, 5/6 -> 2, 7/8 -> 3, 9+ -> 4.
// clears >= 9 is checked BEFORE narrowing so a saturated std::uint64_t never
// overflows the int cast; (clears-1)/2 is only evaluated for clears >= 1.
inline int combo_bonus(std::uint64_t clears) noexcept {
    if (clears >= 9) return 4;
    if (clears == 0) return 0;
    return static_cast<int>((clears - 1) / 2);
}

}  // namespace detail

// Stateless transform: takes the prior Chain by value, returns an optional
// reward. No side effects; no hidden state.
inline std::optional<ClearReward> reward(Chain before,
                                         int rows,
                                         bool t_spin) noexcept {
    // Bounds: normal rows 0..4, spin rows 0..3; otherwise invalid.
    if (rows < 0) return std::nullopt;
    if (t_spin) {
        if (rows > 3) return std::nullopt;
    } else {
        if (rows > 4) return std::nullopt;
    }

    // AFTER streak: positive locks extend saturating, zero lines reset.
    Chain after;
    if (rows > 0) {
        after.clears = study_score::saturating_add(before.clears, static_cast<std::uint64_t>(1));
    } else {
        after.clears = 0;  // resets even for a T-spin 0
    }

    // Base attack from the variant table.
    static const int k_normal[5] = {0, 0, 1, 2, 4};
    static const int k_spin[4]   = {0, 2, 4, 6};
    const int base = t_spin ? k_spin[rows] : k_normal[rows];

    // Combo bonus only for clearing locks, based on the AFTER streak.
    const int bonus = (rows > 0) ? detail::combo_bonus(after.clears) : 0;

    ClearReward out;
    out.chain = after;
    out.attack = base + bonus;
    return out;
}

}  // namespace study_history
