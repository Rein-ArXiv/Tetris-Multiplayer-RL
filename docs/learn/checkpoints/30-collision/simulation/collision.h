#pragma once

#include "simulation/grid.h"
#include "simulation/movement.h"
#include "simulation/piece.h"

namespace study_collision {

// Placement describes how a piece would sit on a *fixed* board.
// Precedence matters: `outside` outranks `occupied`, independent of the
// order in which the piece's four cells happen to be stored.
enum class Placement {
    clear,
    outside,
    occupied,
    unrepresentable
};

// classify() answers a single placement query: which piece cells, if any,
// leave the board or land on a filled cell.
// It never writes to the board and never mutates the piece.
// Order:
//   1. to_board() failure              => unrepresentable
//   2. any cell outside the grid       => outside   (all cells, checked first)
//   3. any cell on a non-empty cell    => occupied  (separate second pass)
//   4. otherwise                       => clear
inline Placement classify(const study_grid::Grid& board,
                          const study_piece::Piece& piece) noexcept {
    const auto cells = study_piece::to_board(piece);
    if (!cells.has_value()) {
        return Placement::unrepresentable;
    }

    // Pass 1: bounds. `outside` must win over `occupied` for every cell.
    for (const auto& cell : *cells) {
        if (!study_grid::Grid::contains(cell.row, cell.column)) {
            return Placement::outside;
        }
    }

    // Pass 2: occupancy. Only reached when every cell is inside the grid.
    for (const auto& cell : *cells) {
        if (!board.is_empty(cell.row, cell.column)) {
            return Placement::occupied;
        }
    }

    return Placement::clear;
}

// try_shift() is the board-aware move. It reuses the existing boundary-only
// study_movement::try_shift() for the actual int arithmetic and adds the
// occupancy check; it does not duplicate bounds math.
// The board is read-only, and `current` stays untouched unless the whole move
// is legal.
inline study_movement::Result try_shift(const study_grid::Grid& board,
                                        study_piece::Piece& current,
                                        int direction) noexcept {
    // Accept one-cell directions and idle; larger steps would skip path checks.
    if (direction < -1 || direction > 1) {
        return study_movement::Result::invalid;
    }

    // A piece that is not cleanly placed cannot be shifted.
    if (classify(board, current) != Placement::clear) {
        return study_movement::Result::invalid;
    }

    // Work on a copy: the original piece must stay intact until every check
    // has passed.
    study_piece::Piece candidate = current;

    const study_movement::Result bounds_result =
        study_movement::try_shift(candidate, direction);
    if (bounds_result != study_movement::Result::moved) {
        return bounds_result; // idle / blocked / invalid from the bounds helper
    }

    // On `moved` the bounds helper guarantees all cells are inside the board,
    // so a non-clear result here can only be `occupied` (or an impossible
    // state, which we treat as invalid).
    const Placement after = classify(board, candidate);
    if (after == Placement::occupied) {
        return study_movement::Result::blocked;
    }
    if (after != Placement::clear) {
        return study_movement::Result::invalid;
    }

    current = candidate; // commit only after all checks pass
    return study_movement::Result::moved;
}

} // namespace study_collision
