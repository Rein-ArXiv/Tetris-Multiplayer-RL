#pragma once

#include "simulation/collision.h"
#include "simulation/grid.h"
#include "simulation/piece.h"

namespace study_spawn {

// A blocked final placement is a normal rule outcome. An unrepresentable
// coordinate sum is an invalid input/state, not a defeat. No mutation or I/O.
enum class Status {
    ready,
    blocked,
    invalid
};

inline Status assess(const study_grid::Grid& board,
                     const study_piece::Piece& piece) noexcept {
    using study_collision::Placement;

    // Exactly one classification call: every branch below is a pure mapping.
    const Placement placement = study_collision::classify(board, piece);

    switch (placement) {
        case Placement::clear:
            return Status::ready;
        case Placement::outside:
        case Placement::occupied:
            return Status::blocked;
        case Placement::unrepresentable:
            return Status::invalid;
    }

    // Defensive final return: an unknown enumerator is not a recognized outcome.
    return Status::invalid;
}

} // namespace study_spawn
