#pragma once
// letterbox.h - pure CPU letterbox layout and coordinate mapping (C++17).
#include <cmath>
#include <cstdint>
#include <optional>
#include <limits>

namespace study_letterbox {

struct Size { int width; int height; };
struct Rect { int x; int y; int width; int height; };
struct Point { double x; double y; };
struct Layout { Size window; Size drawable; Size logical; Rect viewport; };

// Dimensions are plain ints; keep them within 32 bits.
static_assert(std::numeric_limits<int>::digits <= 31, "int must be at most 32 bits");

namespace detail {
inline bool positive(Size s) noexcept { return s.width > 0 && s.height > 0; }
}  // namespace detail

// Fit the logical aspect inside drawable (int64 math), floor the fitted side,
// then center it with floor division. viewport.x/y are top-left drawable coords.
inline std::optional<Layout> make_layout(Size window, Size drawable, Size logical) noexcept {
    if (!detail::positive(window) || !detail::positive(drawable) || !detail::positive(logical))
        return std::nullopt;

    const std::int64_t w = drawable.width;
    const std::int64_t h = drawable.height;
    const std::int64_t lw = logical.width;
    const std::int64_t lh = logical.height;

    int vw;
    int vh;
    if (w * lh <= h * lw) {  // drawable no wider than logical: width binds
        vw = drawable.width;
        vh = static_cast<int>(w * lh / lw);  // floor
    } else {                                 // height binds
        vh = drawable.height;
        vw = static_cast<int>(h * lw / lh);  // floor
    }
    if (vw <= 0 || vh <= 0)
        return std::nullopt;  // never distort by clamping a side to 1

    Layout out{window, drawable, logical, {0, 0, vw, vh}};
    out.viewport.x = (drawable.width - vw) / 2;  // non-negative, floored
    out.viewport.y = (drawable.height - vh) / 2;
    return out;
}

// All mapping helpers require a Layout returned by make_layout, unchanged.
// OpenGL row 0 is the bottom: measure the unused gap from the bottom edge.
inline int gl_y(const Layout& l) noexcept {
    return l.drawable.height - l.viewport.y - l.viewport.height;
}

// Window (screen) -> continuous logical units. Half-open viewport and result
// tests; independent double scales avoid any integer overflow.
inline std::optional<Point> window_to_logical(const Layout& l, Point p) noexcept {
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return std::nullopt;

    const double dx = p.x * static_cast<double>(l.drawable.width) / l.window.width;
    const double dy = p.y * static_cast<double>(l.drawable.height) / l.window.height;

    if (dx < l.viewport.x || dx >= l.viewport.x + l.viewport.width ||
        dy < l.viewport.y || dy >= l.viewport.y + l.viewport.height)
        return std::nullopt;

    const double lx = (dx - l.viewport.x) * l.logical.width / l.viewport.width;
    const double ly = (dy - l.viewport.y) * l.logical.height / l.viewport.height;
    if (lx < 0.0 || lx >= l.logical.width || ly < 0.0 || ly >= l.logical.height)
        return std::nullopt;  // reject rounding drift past the far edge

    return Point{lx, ly};
}

// Logical -> window. Accepts the closed logical rectangle so edges stay usable
// for geometry. Hit tests use half-open bounds; floating-point round trips
// at exact edges are not a reliable inside/outside classifier.
inline std::optional<Point> logical_to_window(const Layout& l, Point p) noexcept {
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return std::nullopt;
    if (p.x < 0.0 || p.x > l.logical.width || p.y < 0.0 || p.y > l.logical.height)
        return std::nullopt;

    const double dx = l.viewport.x + p.x * l.viewport.width / l.logical.width;
    const double dy = l.viewport.y + p.y * l.viewport.height / l.logical.height;
    return Point{dx * l.window.width / l.drawable.width,
                 dy * l.window.height / l.drawable.height};
}

}  // namespace study_letterbox
