#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "simulation/collision.h"

namespace study_kick_search {

// A single kick offset, relative to the pre-attempt piece origin.
struct Offset {
    int row = 0;
    int column = 0;
};

// An ordered list of candidate offsets. Only the first `count` entries are
// meaningful; `count` must be in [1, 7].
struct Policy {
    std::array<Offset, 7> offsets{};
    std::size_t count = 0;
};

// A successful kick: the placement that cleared and the offset's index.
struct Candidate {
    study_piece::Piece piece;
    std::size_t index;
};

// Returns the first offset in policy order that places `rotated` clear on
// `board`. Each offset is applied to the original origin, so offsets always
// share the same baseline and never accumulate. `rotated` and `board` are never mutated.
// Rotation legality is the caller's concern; this tests only the final
// placement, not the path swept between offsets.
inline std::optional<Candidate> first_clear(
    const study_grid::Grid& board, const study_piece::Piece& rotated,
    const Policy& policy) noexcept {
    // A policy must offer between 1 and 7 offsets.
    if (policy.count == 0 || policy.count > policy.offsets.size()) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < policy.count; ++index) {
        const Offset& offset = policy.offsets[index];

        // Start from a fresh copy every iteration; offsets do not accumulate.
        study_piece::Piece candidate = rotated;

        // Widen the origin before adding so the addition cannot overflow.
        const std::int64_t row =
            static_cast<std::int64_t>(rotated.origin.row) +
            static_cast<std::int64_t>(offset.row);
        const std::int64_t column =
            static_cast<std::int64_t>(rotated.origin.column) +
            static_cast<std::int64_t>(offset.column);

        // Skip offsets whose result does not fit in int.
        if (row < std::numeric_limits<int>::min() ||
            row > std::numeric_limits<int>::max() ||
            column < std::numeric_limits<int>::min() ||
            column > std::numeric_limits<int>::max()) {
            continue;
        }

        candidate.origin.row = static_cast<int>(row);
        candidate.origin.column = static_cast<int>(column);

        // Accept only a fully clear placement; reject outside, occupied and
        // unrepresentable alike.
        if (study_collision::classify(board, candidate) ==
            study_collision::Placement::clear) {
            return Candidate{candidate, index};
        }
    }

    return std::nullopt;
}

}  // namespace study_kick_search
