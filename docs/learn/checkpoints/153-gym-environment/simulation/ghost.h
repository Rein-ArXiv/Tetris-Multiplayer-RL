#pragma once

#include <optional>

#include "simulation/collision.h"
#include "simulation/gravity.h"
#include "simulation/grid.h"
#include "simulation/movement.h"
#include "simulation/piece.h"

namespace study_ghost {

// A projected resting placement: the last legal one-row drop of `start`
// together with the number of rows it took to get there.
struct Landing {
    study_piece::Piece piece;
    int distance = 0;
};

// project() answers "where would this piece come to rest?" without ticking
// gravity, a timer, a Round, or a lock. It validates that `start` is cleanly
// placed, then repeatedly asks study_gravity::try_down() to drop a private
// copy:
//   - every `moved` commits the new placement and increments distance
//   - the first `blocked` returns that last valid placement
//   - any `invalid`, or a start that is not clear, returns nullopt
// A finite bound also catches a broken one-row movement contract. A cleanly placed four-cell piece has all four
// cells inside the board, so its top row can advance at most 19 times; the
// 20th drop attempt must be blocked, which makes the kRows budget both
// sufficient and safe.
//
// The board is read-only and `start` is const; the only mutated objects are
// the local Landing and the private `cursor` copy.
inline std::optional<Landing> project(const study_grid::Grid& board,
                                      const study_piece::Piece& start) noexcept {
    // Only a cleanly placed piece may be projected. This rejects shapes that
    // leave the board and shapes overlapping filled cells.
    if (study_collision::classify(board, start) !=
        study_collision::Placement::clear) {
        return std::nullopt;
    }

    // A clear start already blocked below will return distance zero.
    Landing landing{start, 0};
    study_piece::Piece cursor = start;  // private copy; `start` stays const

    for (int step = 0; step < study_grid::Grid::kRows; ++step) {
        const study_movement::Result result =
            study_gravity::try_down(board, cursor);
        if (result == study_movement::Result::moved) {
            landing.piece = cursor;  // try_down commits only on success
            ++landing.distance;
            continue;
        }
        if (result == study_movement::Result::blocked) {
            return landing;  // obstacle reached: report the last valid landing
        }
        return std::nullopt;  // invalid: fail closed, change nothing
    }

    // Budget exhausted without a blocked result: fail closed rather than guess.
    return std::nullopt;
}

}  // namespace study_ghost
