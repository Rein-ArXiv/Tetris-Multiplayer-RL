#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace platform_detail {
// Map a window coordinate to an integer logical coordinate. Floor preserves
// negative positions outside the left/top edge; truncation toward zero does not.
// Saturate only values outside int range (e.g. captured mouse far off-window).
inline int logical_mouse_axis(int coordinate, int offset, int extent, int logical_extent) noexcept
{
    static_assert(std::numeric_limits<int>::digits <= 31, "requires at most 32-bit int");
    if (extent <= 0 || logical_extent <= 0) return -1;
    const std::int64_t delta = static_cast<std::int64_t>(coordinate) - offset;
    const std::int64_t numerator = delta * logical_extent;
    std::int64_t result = numerator / extent;
    if (numerator < 0 && numerator % extent != 0) --result;
    return static_cast<int>(std::clamp(result,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}
} // namespace platform_detail
