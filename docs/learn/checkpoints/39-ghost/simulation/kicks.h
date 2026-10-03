#pragma once
#include "simulation/rotation.h"
#include "simulation/kick_search.h"

namespace study_kicks {
// Teaching policy, not SRS: final placements up to two cells from the original
// origin. Order is gameplay data. Quarter is the SOURCE orientation.
inline std::optional<study_kick_search::Policy> policy(study_catalog::Kind kind,
                                                      int quarter) noexcept {
    if (!study_catalog::find(kind) || quarter < 0 || quarter > 3) return std::nullopt;
    study_kick_search::Policy list;
    list.count = kind == study_catalog::Kind::O ? 1 : 7;
    const int side = (quarter == 0 || quarter == 3) ? -1 : 1;
    list.offsets = {{{0,0},{0,side},{0,-side},{0,2*side},{0,-2*side},{-1,0},{-2,0}}};
    return list;
}

struct Outcome {
    study_rotation::Result result = study_rotation::Result::invalid;
    // Zero is the in-place candidate; -1 means that none was committed.
    int candidate_index = -1;
};

inline Outcome try_clockwise(const study_grid::Grid& board, study_piece::Piece& piece,
                              study_catalog::Kind kind, int& quarter) noexcept {
    const auto expected = study_rotation::shape_at(kind,quarter);
    const auto list = policy(kind,quarter);
    if (!expected || !list || !study_rotation::same_cells(piece.local,*expected) ||
        study_collision::classify(board,piece)!=study_collision::Placement::clear)
        return {};
    const int next_quarter = (quarter+1)%4;
    const auto shape = study_rotation::shape_at(kind,next_quarter);
    if (!shape) return {};
    auto rotated = piece;
    rotated.local = *shape;
    const auto choice = study_kick_search::first_clear(board,rotated,*list);
    if (!choice) return {study_rotation::Result::blocked,-1};
    piece = choice->piece;
    quarter = next_quarter;
    return {study_rotation::Result::rotated,static_cast<int>(choice->index)};
}
} // namespace study_kicks
