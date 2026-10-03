#pragma once
#include "simulation/rotation.h"

namespace study_spin {
// Query the still-active T before locking/clearing. The caller owns history.
// false is a valid non-spin; nullopt means an invalid kind/pose/placement.
inline std::optional<bool> classify(const study_grid::Grid& board,
                                    const study_piece::Piece& piece,
                                    study_catalog::Kind kind, int quarter,
                                    bool rotation_ready) noexcept {
    const auto shape = study_rotation::shape_at(kind,quarter);
    if (!shape || !study_rotation::same_cells(*shape,piece.local) ||
        study_collision::classify(board,piece) != study_collision::Placement::clear)
        return std::nullopt;
    if (kind != study_catalog::Kind::T || !rotation_ready) return false;
    // T's fixed local pivot is (1,1), not its current bounding-box center.
    // Widen before adding, even though the valid-pose contract bounds this origin.
    const std::int64_t pr = std::int64_t{piece.origin.row} + 1;
    const std::int64_t pc = std::int64_t{piece.origin.column} + 1;
    int blocked = 0;
    for (int dr : {-1,1}) for (int dc : {-1,1}) {
        const auto r=pr+dr,c=pc+dc;
        if (r<0 || r>=study_grid::Grid::kRows || c<0 || c>=study_grid::Grid::kColumns)
            ++blocked;
        else if (board.get(static_cast<int>(r),static_cast<int>(c)) == study_grid::Cell::filled)
            ++blocked;
    }
    return blocked >= 3;
}
} // namespace study_spin
