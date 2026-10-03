#pragma once

#include <cstdint>
#include <limits>

#include "simulation/collision.h"

namespace study_gravity {

// Gravity is a periodic, discrete pull toward increasing row indices.
// Counter tracks how many ticks have elapsed in the current interval.
struct Counter {
    int elapsed = 0;    // ticks already spent in the current interval
    int interval = 30;  // ticks per gravity step; must stay positive
};

// Outcome of a single gravity tick.
enum class TickResult {
    waiting,  // interval not finished yet
    moved,    // the piece fell by one row
    blocked,  // this one-row attempt is blocked; sideways input may free it
    invalid   // bad counter or piece state; nothing was changed
};

// try_down() is the downward sibling of study_collision::try_shift().
// It drops the piece one row when the landing cells are free and reports
// `blocked` when they leave the grid or are already occupied.
// This educational checkpoint stops at obstacles; it never locks a piece.
// On every failure `current` is left completely untouched.
inline study_movement::Result try_down(const study_grid::Grid& board,
                                       study_piece::Piece& current) noexcept {
    // Only a cleanly placed piece may be dropped.
    if (study_collision::classify(board, current) !=
        study_collision::Placement::clear) {
        return study_movement::Result::invalid;
    }

    // Work on a copy; commit only after all checks pass.
    study_piece::Piece candidate = current;

    // Widen the origin row to 64 bits before adding so the overflow check
    // cannot itself overflow.
    const std::int64_t widened =
        static_cast<std::int64_t>(candidate.origin.row) + 1;
    if (widened < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
        widened > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        return study_movement::Result::invalid;
    }
    candidate.origin.row = static_cast<int>(widened);

    // Classify the dropped candidate against the read-only board.
    const study_collision::Placement after =
        study_collision::classify(board, candidate);
    if (after == study_collision::Placement::outside ||
        after == study_collision::Placement::occupied) {
        return study_movement::Result::blocked; // obstacle reached
    }
    if (after != study_collision::Placement::clear) {
        return study_movement::Result::invalid; // unrepresentable mapping
    }

    current = candidate; // commit only after all checks pass
    return study_movement::Result::moved;
}

// tick() advances the gravity timer by exactly one call.
// Order:
//   1. Reject a bad counter (interval <= 0 or elapsed outside [0, interval))
//      or a piece that is not cleanly placed => invalid, nothing changes.
//   2. While elapsed < interval - 1, count the tick and report waiting.
//   3. On the final tick, attempt one drop. `invalid` keeps the counter
//      untouched; `moved` and `blocked` both restart the interval.
inline TickResult tick(const study_grid::Grid& board,
                       study_piece::Piece& piece,
                       Counter& counter) noexcept {
    // Validate the timer and the piece before touching anything.
    if (counter.interval <= 0 || counter.elapsed < 0 ||
        counter.elapsed >= counter.interval) {
        return TickResult::invalid;
    }
    if (study_collision::classify(board, piece) !=
        study_collision::Placement::clear) {
        return TickResult::invalid;
    }

    // The interval is not over yet: just record this tick.
    if (counter.elapsed < counter.interval - 1) {
        ++counter.elapsed;
        return TickResult::waiting;
    }

    // Final tick of the interval: try to fall.
    const study_movement::Result result = try_down(board, piece);
    if (result == study_movement::Result::invalid) {
        return TickResult::invalid; // counter and piece preserved
    }

    counter.elapsed = 0; // restart the interval on both moved and blocked
    return result == study_movement::Result::moved ? TickResult::moved
                                                   : TickResult::blocked;
}

}  // namespace study_gravity
