#pragma once
// Minimal scoring helpers for the teaching simulation (C++17).

#include <cstdint>
#include <optional>
#include <limits>

namespace study_score {

struct Totals {
    std::uint64_t points = 0;
    std::uint64_t lines = 0;
};

// Level grows every 10 cleared lines and is capped at 20.
constexpr unsigned level(const Totals& totals) noexcept {
    return totals.lines >= 190u ? 20u : 1u + static_cast<unsigned>(totals.lines / 10u);
}

// Add without wrapping: clamp to UINT64_MAX on overflow.
constexpr std::uint64_t saturating_add(std::uint64_t a, std::uint64_t b) noexcept {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    return b > maximum - a ? maximum : a + b;
}

// Points for 0..4 rows at a 1..20 level, or nullopt when out of range.
inline std::optional<std::uint64_t> normal_points(int rows, unsigned level) noexcept {
    if (rows < 0 || rows > 4 || level < 1 || level > 20) {
        return std::nullopt;
    }
    constexpr std::uint64_t table[5] = {0u, 100u, 300u, 600u, 1000u};
    return table[rows] * static_cast<std::uint64_t>(level);
}

inline std::optional<std::uint64_t> spin_points(int rows, unsigned level) noexcept {
    if (rows < 0 || rows > 3 || level < 1 || level > 20) return std::nullopt;
    constexpr std::uint64_t table[] = {400,800,1200,1600};
    return table[rows] * level;
}

// Award a clear using the level before the clear; input is left untouched.
inline std::optional<Totals> award(const Totals& before, int rows, bool t_spin = false) noexcept {
    const auto points = t_spin ? spin_points(rows,level(before)) : normal_points(rows,level(before));
    if (!points) return std::nullopt;
    Totals after = before;
    after.points = saturating_add(before.points, *points);
    after.lines = saturating_add(before.lines, static_cast<std::uint64_t>(rows));
    return after;
}

// Simulation ticks per automatic drop, from 30 at level 1 to 3 at level 20.
inline std::optional<int> gravity_interval(unsigned level) noexcept {
    if (level < 1 || level > 20) {
        return std::nullopt;
    }
    return 30 - static_cast<int>(level - 1) * 27 / 19;
}

}  // namespace study_score
