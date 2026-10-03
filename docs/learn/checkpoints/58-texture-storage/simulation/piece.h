#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <optional>

#include "simulation/grid.h"

namespace study_piece {

// The int arithmetic below relies on a 32-bit-or-narrower int.
static_assert(std::numeric_limits<int>::digits <= 31,
              "study_piece requires a 32-bit or smaller int");

struct LocalCell {
    int row = 0;
    int column = 0;
};

struct Origin {
    int row = 0;
    int column = 0;
};

using Shape = std::array<LocalCell, 4>;
using BoardCells = std::array<study_grid::Position, 4>;

// Default T-tetromino in local coordinates.
// Occupied bounds: rows 0..1, columns 0..2. Origin local (0,0) is empty;
// the T rotation pivot at local (1,1) is a separate concept, not stored here.
inline constexpr Shape t_shape{{
    LocalCell{0, 1},
    LocalCell{1, 0},
    LocalCell{1, 1},
    LocalCell{1, 2},
}};

// Piece owns its local cells by value and its origin by value.
struct Piece {
    Shape local = t_shape;
    Origin origin{};
};

// Inclusive extrema of the four local cells.
struct Bounds {
    int min_row = 0;
    int min_column = 0;
    int max_row = 0;
    int max_column = 0;
};

// Inclusive extrema computed with direct comparisons only. No
// (max - min + 1) sizing math; the extrema are reported as data and never
// recenter the stored cells.
inline Bounds local_bounds(const Shape& shape) noexcept {
    Bounds bounds{shape[0].row, shape[0].column, shape[0].row, shape[0].column};
    for (std::size_t i = 1; i < shape.size(); ++i) {
        const LocalCell& cell = shape[i];
        if (cell.row < bounds.min_row) {
            bounds.min_row = cell.row;
        }
        if (cell.column < bounds.min_column) {
            bounds.min_column = cell.column;
        }
        if (cell.row > bounds.max_row) {
            bounds.max_row = cell.row;
        }
        if (cell.column > bounds.max_column) {
            bounds.max_column = cell.column;
        }
    }
    return bounds;
}

// Translates each local cell by the origin. Both operands are widened to
// int64 before the addition, so the addition itself cannot overflow. A sum
// outside int's range is an arithmetic failure and yields nullopt; an in-range
// sum outside the 20x10 board is allowed, since this is not a movement-legality
// test. Cell order is preserved, the Piece is not mutated, and no clamping or
// allocation occurs. The returned BoardCells is a detached snapshot, not a
// reference; the caller must not mutate the Piece concurrently with this call.
// Shape validity (duplicates/connectedness) is deliberately not checked here.
inline std::optional<BoardCells> to_board(const Piece& piece) noexcept {
    BoardCells candidate{};
    for (std::size_t i = 0; i < piece.local.size(); ++i) {
        const std::int64_t row =
            static_cast<std::int64_t>(piece.local[i].row) +
            static_cast<std::int64_t>(piece.origin.row);
        const std::int64_t column =
            static_cast<std::int64_t>(piece.local[i].column) +
            static_cast<std::int64_t>(piece.origin.column);
        if (row < std::numeric_limits<int>::min() ||
            row > std::numeric_limits<int>::max() ||
            column < std::numeric_limits<int>::min() ||
            column > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        candidate[i] = study_grid::Position{
            static_cast<int>(row), static_cast<int>(column)};
    }
    return candidate;
}

}  // namespace study_piece
