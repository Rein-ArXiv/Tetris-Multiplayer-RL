#pragma once

// Pure-CPU board geometry for the fixed study grid layout.
// No GL/SDL dependencies. Header-only, no heap allocation.

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

#include "simulation/grid.h"
#include "coordinates.h"
#include "mesh.h"

namespace study_board {

inline constexpr double logical_width = 320.0;
inline constexpr double logical_height = 240.0;
inline constexpr double origin_x = 110.0;
inline constexpr double origin_y = 20.0;
inline constexpr double pitch = 10.0;
inline constexpr double ink = 9.0;
static_assert(pitch > 0 && ink > 0 && ink <= pitch);
static_assert(origin_x >= 0 && origin_y >= 0);
static_assert(origin_x + pitch * study_grid::Grid::kColumns <= logical_width);
static_assert(origin_y + pitch * study_grid::Grid::kRows <= logical_height);

struct Rect {
    double left;
    double top;
    double right;
    double bottom;
};

inline std::optional<Rect> cell_rect(int row, int column) noexcept {
    if (!study_grid::Grid::contains(row, column)) {
        return std::nullopt;
    }
    const double x = origin_x + static_cast<double>(column) * pitch;
    const double y = origin_y + static_cast<double>(row) * pitch;
    return Rect{x, y, x + ink, y + ink};
}

inline std::optional<study_grid::Position> cell_at(double x, double y) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return std::nullopt;
    }
    const double board_right = origin_x + pitch * static_cast<double>(study_grid::Grid::kColumns);
    const double board_bottom = origin_y + pitch * static_cast<double>(study_grid::Grid::kRows);
    if (x < origin_x || x >= board_right) {
        return std::nullopt;
    }
    if (y < origin_y || y >= board_bottom) {
        return std::nullopt;
    }
    const int column = static_cast<int>((x - origin_x) / pitch);
    const int row = static_cast<int>((y - origin_y) / pitch);
    return study_grid::Position{row, column};
}

inline constexpr std::size_t vertex_count = study_grid::Grid::kCount * 6;

struct Mesh {
    std::array<study_mesh::Vertex2, vertex_count> vertices{};
    std::size_t empty_vertices = 0;
};

namespace detail {

inline void push_ui_vertex(Mesh& mesh, std::size_t& write, double x, double y) noexcept {
    const auto clip = study_coordinates::ui_to_clip(x, y, logical_width, logical_height);
    // Called only with the finite, bounded corners produced by cell_rect.
    // Fixed positive logical dimensions guarantee ui_to_clip has a value.
    mesh.vertices[write] = {static_cast<float>(clip->x), static_cast<float>(clip->y)};
    ++write;
}

inline void push_quad(Mesh& mesh, std::size_t& write, const Rect& r) noexcept {
    // BL, BR, TR, BL, TR, TL
    push_ui_vertex(mesh, write, r.left, r.bottom);
    push_ui_vertex(mesh, write, r.right, r.bottom);
    push_ui_vertex(mesh, write, r.right, r.top);
    push_ui_vertex(mesh, write, r.left, r.bottom);
    push_ui_vertex(mesh, write, r.right, r.top);
    push_ui_vertex(mesh, write, r.left, r.top);
}

}  // namespace detail

// Builds a detached snapshot; the caller must rebuild after any Grid edit.
// Grid stores only the two valid Cell values; no concurrent mutation is allowed.
inline Mesh make_mesh(const study_grid::Grid& grid) noexcept {
    Mesh mesh{};
    std::size_t write = 0;
    for (int pass = 0; pass < 2; ++pass) {
        const study_grid::Cell want =
            (pass == 0) ? study_grid::Cell::empty : study_grid::Cell::filled;
        for (int row = 0; row < study_grid::Grid::kRows; ++row) {
            for (int column = 0; column < study_grid::Grid::kColumns; ++column) {
                const auto cell = grid.get(row, column);
                if (*cell != want) {
                    continue;
                }
                const auto rect = cell_rect(row, column);
                detail::push_quad(mesh, write, *rect);
            }
        }
        if (pass == 0) {
            mesh.empty_vertices = write;
        }
    }
    return mesh;
}

}  // namespace study_board