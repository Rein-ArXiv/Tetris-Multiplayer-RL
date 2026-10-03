#pragma once

#include <array>
#include <cmath>
#include <optional>

namespace study_raster {

// Teaching-only CPU oracle for conceptual rasterization. It is not a
// spec-exact GL rasterizer and does not model GPU hardware behavior.
struct Point { double x, y; };
struct Weights { double a, b, c; };

// boundary is diagnostic only: it does not choose shared-edge ownership and
// makes no claim about GL include/exclude rule compliance.
enum class Region { outside, boundary, inside };

struct Sample { Weights weights; Region region; };

// Edge function, twice the signed area of triangle (a, b, p).
inline double edge(Point a, Point b, Point p) noexcept {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

inline bool finite_in_range(double v) noexcept {
    return std::isfinite(v) && std::abs(v) <= 1000000.0;
}

// Returns nullopt for out-of-range or nonfinite inputs, and for degenerate
// (computed zero-area) triangles. No epsilon expansion or subpixel snapping is done.
inline std::optional<Sample> sample_triangle(const std::array<Point, 3>& triangle,
                                             Point p) noexcept {
    for (const Point& v : triangle) {
        if (!finite_in_range(v.x) || !finite_in_range(v.y)) return std::nullopt;
    }
    if (!finite_in_range(p.x) || !finite_in_range(p.y)) return std::nullopt;

    const Point& a = triangle[0];
    const Point& b = triangle[1];
    const Point& c = triangle[2];

    const double area = edge(a, b, c);
    if (area == 0.0) return std::nullopt;

    const double e0 = edge(b, c, p);
    const double e1 = edge(c, a, p);
    const double e2 = edge(a, b, p);

    const Weights w{ e0 / area, e1 / area, e2 / area };
    if (!std::isfinite(w.a) || !std::isfinite(w.b) || !std::isfinite(w.c)) {
        return std::nullopt;
    }

    // Interior points keep the same edge sign as the triangle area, so both
    // windings are accepted by comparing oriented edges against area's sign.
    const bool outside =
        (area > 0.0 && (e0 < 0.0 || e1 < 0.0 || e2 < 0.0)) ||
        (area < 0.0 && (e0 > 0.0 || e1 > 0.0 || e2 > 0.0));

    Region region = Region::inside;
    if (outside) {
        region = Region::outside;
    } else if (e0 == 0.0 || e1 == 0.0 || e2 == 0.0) {
        region = Region::boundary;
    }

    return Sample{ w, region };
}

// Default single-sample pixel center in continuous lower-left window coords.
inline Point pixel_center(int x, int y) noexcept {
    return Point{ static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5 };
}

// A -> red, B -> green, C -> blue. This is an affine/screen-linear blend,
// equal to smooth interpolation for our equal clip-w=1 vertices. Unequal-w
// perspective interpolation would
// need a clip-w correction first; that is out of scope here. Core helper does
// not clamp or hide weights outside [0, 1].
inline std::array<double, 3> mix_rgb(Weights w) noexcept {
    return { w.a, w.b, w.c };
}

} // namespace study_raster
