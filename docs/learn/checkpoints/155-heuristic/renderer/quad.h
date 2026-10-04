#pragma once
// study_quad: bounded CPU teaching helpers for quad meshes and top-left edge ownership.

#include "mesh.h"
#include "raster.h"

#include <algorithm>
#include <array>
#include <optional>

namespace study_quad {

// Two triangles ABC, ACD covering the unit square centered on the origin.
// reverse_first swaps the 2nd and 3rd vertices of the first triangle only;
// shared endpoint positions stay identical; CPU normalization below restores
// opposite directed edges when the first winding is reversed.
using Quad = std::array<study_mesh::Vertex2, 6>;

inline Quad make_quad(bool reverse_first = false) noexcept {
    const study_mesh::Vertex2 A{-0.5f, -0.5f};
    const study_mesh::Vertex2 B{ 0.5f, -0.5f};
    const study_mesh::Vertex2 C{ 0.5f,  0.5f};
    const study_mesh::Vertex2 D{-0.5f,  0.5f};

    if (reverse_first) {
        return Quad{A, C, B, A, C, D};
    }
    return Quad{A, B, C, A, C, D};
}

// Explicit TOP-LEFT fill policy for a CPU teaching sampler in lower-left,
// y-up window coordinates with counter-clockwise vertices: an edge owns a
// boundary point when it is a left edge (dy < 0) or a top edge (dy == 0 && dx < 0).
inline bool included_edge(const study_raster::Point& a,
                          const study_raster::Point& b) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return (dy < 0.0) || (dy == 0.0 && dx < 0.0);
}

// This is our CPU fill policy, not a claim that GL mandates this exact owner.
// Hardware subpixel quantization is not modeled.
// Decide whether p is owned by this triangle under the top-left rule.
// Returns nullopt only for input that sample_triangle rejects as invalid or
// degenerate, false for outside, true for strictly inside.
inline std::optional<bool> covers_top_left(
    std::array<study_raster::Point, 3> triangle,
    study_raster::Point p) noexcept {

    const std::optional<study_raster::Sample> sample =
        study_raster::sample_triangle(triangle, p);

    if (!sample.has_value()) {
        return std::nullopt;  // invalid input or degenerate computed area
    }
    if (sample->region == study_raster::Region::outside) {
        return false;
    }
    if (sample->region == study_raster::Region::inside) {
        return true;
    }

    // Boundary: normalize winding to CCW so the directed edges are well defined.
    // edge(A, B, C) is twice the signed area, negative for clockwise input.
    if (study_raster::edge(triangle[0], triangle[1], triangle[2]) < 0.0) {
        std::swap(triangle[1], triangle[2]);
    }

    const study_raster::Point& a = triangle[0];
    const study_raster::Point& b = triangle[1];
    const study_raster::Point& c = triangle[2];

    // Only zero edges impose the extra top-left test; non-boundary edges are
    // already accepted by sample_triangle. A single rejected zero edge fails.
    const auto owned = [&p](const study_raster::Point& from,
                            const study_raster::Point& to) noexcept {
        return (study_raster::edge(from, to, p) != 0.0) || included_edge(from, to);
    };

    // Identical shared endpoints in opposite directions (A..C and C..A) are what
    // let two adjacent triangles agree on exactly one owner per shared edge.
    return owned(a, b) && owned(b, c) && owned(c, a);
}

}  // namespace study_quad
