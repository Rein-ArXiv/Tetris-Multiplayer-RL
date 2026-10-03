#pragma once
// study_coordinates - header-only C++17 CPU-side coordinate math.
//
// Purpose: a lesson that predicts window-space results on the CPU and then
// compares those predictions with what the real GL rasterizer produces.
// Standard library only: <cmath>, <optional>. No SDL, no GL, no matrix class.
//
// Space conventions used below:
//   * ClipPosition is what a vertex shader writes to gl_Position. It is
//     CLIP space, not NDC. Division by w is a separate step.
//   * NdcPosition is post-divide, computed as xyz/w. Its numerical range alone is not a clip acceptance test.
//   * WindowPosition is continuous window coordinates. Accepted clip points map depth to
//     [0,1] under the default glDepthRange; outside inputs are not clamped here. It is NOT an integer pixel index.

#include <cmath>
#include <optional>

namespace study_coordinates {

struct ClipPosition { double x, y, z, w; };
struct NdcPosition  { double x, y, z; };
struct Viewport     { int x, y, width, height; };
struct WindowPosition { double x, y, depth; };

namespace detail {

inline bool finite(double v) noexcept { return std::isfinite(v); }

inline bool finite4(double a, double b, double c, double d) noexcept {
    return std::isfinite(a) && std::isfinite(b) &&
           std::isfinite(c) && std::isfinite(d);
}

} // namespace detail

// 1) inside_clip_volume
//
// Predicate for ONE point against the default OpenGL 3.3 clip volume:
//     -w <= x <= w,  -w <= y <= w,  -w <= z <= w,  with w > 0.
// The bounds are inclusive.
//
// This is NOT triangle clipping and NOT a visibility test. A triangle may have
// vertices outside the volume and still intersect it; the hardware clips
// edges, it does not reject the whole primitive per-vertex.
//
// We reject w == 0 because our following divide requires nonzero w.
// The all-zero tuple satisfies the written inequalities but cannot be divided.
// This teaching helper does not specify GL behavior for singular primitives.
inline bool inside_clip_volume(const ClipPosition& p) noexcept {
    if (!detail::finite4(p.x, p.y, p.z, p.w)) return false;
    if (!(p.w > 0.0)) return false;
    return (p.x >= -p.w && p.x <= p.w &&
            p.y >= -p.w && p.y <= p.w &&
            p.z >= -p.w && p.z <= p.w);
}

// 2) perspective_divide
//
// Perspective divide: (x/w, y/w, z/w). This step decides nothing about clip
// acceptance or visibility.
//
// Non-finite input is rejected, as is w == 0. A NEGATIVE w is permitted
// arithmetically: the division is well defined and changes the signs of the three quotients;
// whether the result is visible is answered elsewhere. Non-finite quotients
// (for example, overflow to infinity) are rejected.
inline std::optional<NdcPosition> perspective_divide(const ClipPosition& p) noexcept {
    if (!detail::finite4(p.x, p.y, p.z, p.w)) return std::nullopt;
    if (p.w == 0.0) return std::nullopt;

    const NdcPosition n{ p.x / p.w, p.y / p.w, p.z / p.w };

    if (!detail::finite(n.x) || !detail::finite(n.y) || !detail::finite(n.z))
        return std::nullopt;
    return n;
}

// 3) to_window
//
// Maps NDC to continuous window coordinates:
//     wx    = origin_x + (n.x * 0.5 + 0.5) * width
//     wy    = origin_y + (n.y * 0.5 + 0.5) * height
//     depth = n.z * 0.5 + 0.5        // default GL depth range [0,1]
//
// Both dimensions must be positive; the origin may be negative. Integers are
// converted to double BEFORE any addition, so integer overflow cannot occur.
//
// NDC values outside [-1,1] are accepted and map outside the viewport: this is
// a coordinate mapping, NOT a visibility check. Results are continuous window
// coordinates, not integer pixel indices, and hardware viewport clamping is
// deliberately not modeled. Non-finite results are rejected.
inline std::optional<WindowPosition> to_window(const NdcPosition& n,
                                               const Viewport& v) noexcept {
    if (!detail::finite(n.x) || !detail::finite(n.y) || !detail::finite(n.z))
        return std::nullopt;
    if (v.width <= 0 || v.height <= 0) return std::nullopt;

    // Cast first: every later term is double, so ints never participate in the
    // arithmetic that could overflow.
    const double origin_x = static_cast<double>(v.x);
    const double origin_y = static_cast<double>(v.y);
    const double width    = static_cast<double>(v.width);
    const double height   = static_cast<double>(v.height);

    const WindowPosition w{
        origin_x + (n.x * 0.5 + 0.5) * width,
        origin_y + (n.y * 0.5 + 0.5) * height,
        n.z * 0.5 + 0.5
    };

    if (!detail::finite(w.x) || !detail::finite(w.y) || !detail::finite(w.depth))
        return std::nullopt;
    return w;
}

// 4) ui_to_clip
//
// Maps a top-left-origin logical UI point into clip space:
//     x_clip = 2 * (x / width)  - 1
//     y_clip = 1 - 2 * (y / height)
//     z_clip = 0, w_clip = 1
//
// The result is gl_Position, i.e. CLIP coordinates, not NDC. It is named
// ui_to_clip for that reason; claiming gl_Position is universally NDC would
// be wrong.
//
// All arguments must be finite and both dimensions strictly positive. Dividing
// before multiplying keeps intermediate magnitudes small and avoids needless
// overflow. Points outside the UI rectangle are allowed and are not clamped.
//
// No epsilon is applied to the zero check: a zero dimension is an invalid
// mapping (a degenerate projection), which is a different concern from
// floating-point precision. Zero is rejected exactly, by policy.
inline std::optional<ClipPosition> ui_to_clip(double x, double y,
                                              double logical_width,
                                              double logical_height) noexcept {
    if (!detail::finite4(x, y, logical_width, logical_height)) return std::nullopt;
    if (!(logical_width > 0.0) || !(logical_height > 0.0)) return std::nullopt;

    const ClipPosition c{
        2.0 * (x / logical_width) - 1.0,
        1.0 - 2.0 * (y / logical_height),
        0.0,
        1.0
    };

    if (!detail::finite4(c.x, c.y, c.z, c.w)) return std::nullopt;
    return c;
}

} // namespace study_coordinates
