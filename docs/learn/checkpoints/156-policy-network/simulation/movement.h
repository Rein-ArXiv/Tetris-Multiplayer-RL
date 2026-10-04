#ifndef SIMULATION_MOVEMENT_H
#define SIMULATION_MOVEMENT_H

#include <cstdint>
#include <limits>

#include "simulation/piece.h"
#include "simulation/grid.h"

namespace study_movement {

using study_piece::Piece;

// Outcome of an attempted horizontal translation of a piece.
enum class Result { idle, moved, blocked, invalid };

// Resolve the horizontal direction requested by the two opposing inputs.
// Both press edges or neither press edge yields no movement (0); otherwise the single
// pressed key yields -1 for left and +1 for right.
inline int horizontal_intent(bool left_pressed, bool right_pressed) noexcept {
    if (left_pressed == right_pressed) {
        return 0;
    }
    return left_pressed ? -1 : 1;
}

// True only when every occupied cell of the piece maps onto the board.
// The origin itself is not required to lie inside the board.
inline bool inside_board(const Piece& piece) noexcept {
    const auto cells = study_piece::to_board(piece);
    if (!cells.has_value()) {
        return false;
    }
    for (const auto& position : *cells) {
        if (!study_grid::Grid::contains(position.row, position.column)) {
            return false;
        }
    }
    return true;
}

// Attempt to move the piece one column in `direction` (-1 left, +1 right).
// A candidate is built and validated before it is committed, so every failure
// and every no-op leaves all fields of `current` unchanged.
inline Result try_shift(Piece& current, int direction) noexcept {
    // Only unit horizontal steps are meaningful to this layer.
    if (direction < -1 || direction > 1) {
        return Result::invalid;
    }

    // This exercise requires all current cells inside the board.
    // Hidden spawn rows and recovery from an outside state are separate policies.
    if (!inside_board(current)) {
        return Result::invalid;
    }

    if (direction == 0) {
        return Result::idle;
    }

    // Work on a copy so the caller's piece is never partially modified.
    Piece candidate = current;

    // Widen the origin column to 64 bits before adding so the range check
    // cannot itself overflow.
    const std::int64_t widened =
        static_cast<std::int64_t>(candidate.origin.column) +
        static_cast<std::int64_t>(direction);
    if (widened < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
        widened > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        return Result::invalid;
    }
    candidate.origin.column = static_cast<int>(widened);

    // Recompute the occupied cells for the shifted origin; a translation
    // failure here means the coordinate mapping is not usable.
    const auto cells = study_piece::to_board(candidate);
    if (!cells.has_value()) {
        return Result::invalid;
    }
    for (const auto& position : *cells) {
        if (!study_grid::Grid::contains(position.row, position.column)) {
            return Result::blocked;
        }
    }

    // Commit only after the candidate has been fully validated.
    current = candidate;
    return Result::moved;
}

}  // namespace study_movement

#endif  // SIMULATION_MOVEMENT_H
