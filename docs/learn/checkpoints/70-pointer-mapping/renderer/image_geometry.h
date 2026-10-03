#pragma once

// study_image_quad: pure-CPU quad geometry for a 2D image badge.
//
// Coordinate contract:
//   * Logical space has +x to the right and +y downward.
//   * Draw::rect gives the unrotated top-left corner and size.
//   * Draw::uv selects a sub-rectangle of the source image; endpoints may be
//     reversed or equal, which flips or collapses sampling without clamping.
//   * Draw::tint and pivots are normalized to [0, 1].
//   * A positive Draw::angle_degrees rotates clockwise in logical space.
//   * Rotation happens about the normalized pivot inside the rect, then the
//     logical position is projected to NDC. The visible logical rectangle maps
//     to [-1, 1]; offscreen positions may lie outside that interval.
// Nothing here allocates, touches a GPU, or depends on a windowing library.

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <type_traits>

namespace study_image_quad {

struct Rect {
    float x = 20.0f;
    float y = 20.0f;
    float width = 48.0f;
    float height = 48.0f;
};

struct Uv {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct Tint {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Draw {
    Rect rect;
    Uv uv;
    Tint tint;
    float pivot_x = 0.5f;
    float pivot_y = 0.5f;
    float angle_degrees = 0.0f;
};

// Interleaved vertex consumed by a straightforward 2D sprite shader.
struct Vertex {
    float x;
    float y;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
};

// The vertex ABI is fixed: 8 floats, UV at byte 8, color at byte 16.
static_assert(std::is_standard_layout_v<Vertex>, "Vertex must be standard layout");
static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes");
static_assert(offsetof(Vertex, u) == 8, "UV must start at byte 8");
static_assert(offsetof(Vertex, r) == 16, "color must start at byte 16");


// A normalized coordinate is valid only when finite and inside [0, 1].
inline bool unit(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

// Expands a Draw into two triangles (six vertices).
//
// The four logical corners are top-left, top-right, bottom-right, bottom-left.
// Each corner keeps the UV endpoint that belongs to it, and the tint is copied
// verbatim. The triangle list follows the fixed corner indices
// {0, 3, 2, 0, 2, 1}. Invalid input yields nullopt before any trigonometry is
// evaluated; finite but far offscreen geometry is still accepted.
inline std::optional<std::array<Vertex, 6>> make_vertices(
    const Draw& draw,
    float screen_width = 320.0f,
    float screen_height = 240.0f) noexcept {
    const Rect& rect = draw.rect;
    const Uv& uv = draw.uv;
    const Tint& tint = draw.tint;

    const bool valid =
        std::isfinite(screen_width) && screen_width > 0.0f &&
        std::isfinite(screen_height) && screen_height > 0.0f &&
        std::isfinite(rect.x) && std::isfinite(rect.y) &&
        std::isfinite(rect.width) && rect.width > 0.0f &&
        std::isfinite(rect.height) && rect.height > 0.0f &&
        unit(uv.u0) && unit(uv.v0) && unit(uv.u1) && unit(uv.v1) &&
        unit(tint.r) && unit(tint.g) && unit(tint.b) && unit(tint.a) &&
        unit(draw.pivot_x) && unit(draw.pivot_y) &&
        std::isfinite(draw.angle_degrees);
    if (!valid) {
        return std::nullopt;
    }

    // Double intermediates keep the rotation and projection stable; the angle
    // is reduced before it reaches sin/cos.
    const double degrees = std::remainder(static_cast<double>(draw.angle_degrees), 360.0);
    const double radians = degrees * (3.14159265358979323846 / 180.0);
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);

    const double width = static_cast<double>(rect.width);
    const double height = static_cast<double>(rect.height);
    const double pivot_x = static_cast<double>(rect.x) + static_cast<double>(draw.pivot_x) * width;
    const double pivot_y = static_cast<double>(rect.y) + static_cast<double>(draw.pivot_y) * height;

    // Corner (s, t) is expressed in unit rect space; UV endpoints are bound to
    // their matching corner so flips and constant sampling survive untouched.
    struct Corner {
        double s;
        double t;
        double u;
        double v;
    };
    const Corner corner_defs[4] = {
        {0.0, 0.0, static_cast<double>(uv.u0), static_cast<double>(uv.v0)}, // top-left
        {1.0, 0.0, static_cast<double>(uv.u1), static_cast<double>(uv.v0)}, // top-right
        {1.0, 1.0, static_cast<double>(uv.u1), static_cast<double>(uv.v1)}, // bottom-right
        {0.0, 1.0, static_cast<double>(uv.u0), static_cast<double>(uv.v1)}, // bottom-left
    };

    const double max_float = static_cast<double>((std::numeric_limits<float>::max)());

    std::array<Vertex, 4> corners{};
    for (int i = 0; i < 4; ++i) {
        const Corner& corner = corner_defs[i];
        const double offset_x = (corner.s - static_cast<double>(draw.pivot_x)) * width;
        const double offset_y = (corner.t - static_cast<double>(draw.pivot_y)) * height;
        const double world_x = cosine * offset_x - sine * offset_y + pivot_x;
        const double world_y = sine * offset_x + cosine * offset_y + pivot_y;
        const double ndc_x = 2.0 * world_x / static_cast<double>(screen_width) - 1.0;
        const double ndc_y = 1.0 - 2.0 * world_y / static_cast<double>(screen_height);

        // NDC is written as a float, so refuse values that would overflow or
        // turn non-finite during the narrowing cast.
        if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) ||
            ndc_x < -max_float || ndc_x > max_float ||
            ndc_y < -max_float || ndc_y > max_float) {
            return std::nullopt;
        }

        corners[i].x = static_cast<float>(ndc_x);
        corners[i].y = static_cast<float>(ndc_y);
        corners[i].u = static_cast<float>(corner.u);
        corners[i].v = static_cast<float>(corner.v);
        corners[i].r = tint.r;
        corners[i].g = tint.g;
        corners[i].b = tint.b;
        corners[i].a = tint.a;
    }

    // Fixed corner indices produce the two triangles; every emitted vertex is a
    // copy of a corner, so world positions and UVs are never clamped.
    const int index_pattern[6] = {0, 3, 2, 0, 2, 1};
    std::array<Vertex, 6> vertices{};
    for (int i = 0; i < 6; ++i) {
        vertices[i] = corners[index_pattern[i]];
    }
    return vertices;
}

} // namespace study_image_quad
