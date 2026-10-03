#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

// Numerical reference for signed distance and a smooth alpha mask.
// The mathematical formula is a Euclidean SDF; double evaluation is approximate.
namespace study_rounded_distance {

namespace detail {

// Numeric policy: every coordinate-like magnitude is bounded by 1e6.
inline constexpr double kMaxMagnitude = 1.0e6;

inline bool finite(double v) noexcept {
    return std::isfinite(v) != 0;
}

} // namespace detail

// Centered rounded rectangle. Finite half sizes >0, radius in [0,min(half)].
// Coordinates and half sizes are bounded to 1e6 for this numerical study.
// Negative inside, zero on the boundary, positive outside. Radius zero gives
// a sharp rectangle's distance; the rendering caller decides whether to mask it.
inline std::optional<double> signed_distance(double x,
                                             double y,
                                             double half_width,
                                             double half_height,
                                             double radius) noexcept {
    using detail::kMaxMagnitude;
    using detail::finite;

    if (!finite(x) || !finite(y) || !finite(half_width) ||
        !finite(half_height) || !finite(radius)) {
        return std::nullopt;
    }
    if (!(half_width > 0.0) || !(half_height > 0.0)) {
        return std::nullopt;
    }
    if (radius < 0.0) {
        return std::nullopt;
    }

    const double max_radius = (std::min)(half_width, half_height);
    if (radius > max_radius) {
        return std::nullopt;
    }

    const double ax = std::abs(x);
    const double ay = std::abs(y);
    if (ax > kMaxMagnitude || ay > kMaxMagnitude ||
        half_width > kMaxMagnitude || half_height > kMaxMagnitude) {
        return std::nullopt;
    }

    const double qx = ax - half_width + radius;
    const double qy = ay - half_height + radius;

    const double outside = std::hypot((std::max)(qx, 0.0), (std::max)(qy, 0.0));
    const double inside = (std::min)((std::max)(qx, qy), 0.0);
    return outside + inside - radius;
}

// Smooth mask with a finite positive transition width, not exact pixel coverage.
// At -width/2, 0 and +width/2 the result is 1, .5 and 0 respectively.
inline std::optional<double> opacity(double distance,
                                     double transition_width = 1.0) noexcept {
    using detail::finite;

    if (!finite(distance) || !finite(transition_width)) {
        return std::nullopt;
    }
    if (!(transition_width > 0.0)) {
        return std::nullopt;
    }

    const double t = (std::clamp)(distance / transition_width + 0.5, 0.0, 1.0);
    return 1.0 - t * t * (3.0 - 2.0 * t);
}

} // namespace study_rounded_distance

