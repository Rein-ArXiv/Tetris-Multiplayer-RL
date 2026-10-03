#pragma once

namespace study_meta::levels {

inline constexpr int kMaxLevel = 60;
inline constexpr int kBaseCost = 100;
inline constexpr int kCostStep = 20;

// Clamp first so all later math only sees an in-range level.
constexpr int bounded_level(int level) {
    return level < 1 ? 1 : (level > kMaxLevel ? kMaxLevel : level);
}

// Cumulative xp needed to reach a level (total_xp_for_level(1) == 0).
constexpr int total_xp_for_level(int level) {
    const int k = bounded_level(level) - 1;
    return kBaseCost * k + kCostStep * k * (k - 1) / 2;
}

// Xp needed to advance from a level to the next; 0 at max level.
constexpr int xp_to_next(int level) {
    const int l = bounded_level(level);
    return l == kMaxLevel ? 0 : kBaseCost + kCostStep * (l - 1);
}

// Highest level whose cumulative threshold is <= xp; negatives clamp to 0.
inline int level_for_xp(int xp) {
    if (xp < 0) xp = 0;
    int level = 1;
    while (level < kMaxLevel && xp >= total_xp_for_level(level + 1))
        ++level;
    return level;
}

struct Progress {
    int level;  // current level, always 1..kMaxLevel
    int into;   // xp gained inside the current level
    int need;   // xp required to finish the current level
};

// Normalize xp, then report progress; at max level into/need are 0.
inline Progress progress(int xp) {
    if (xp < 0) xp = 0;
    const int level = level_for_xp(xp);
    if (level >= kMaxLevel) return {kMaxLevel, 0, 0};
    return {level, xp - total_xp_for_level(level), xp_to_next(level)};
}

}  // namespace study_meta::levels
