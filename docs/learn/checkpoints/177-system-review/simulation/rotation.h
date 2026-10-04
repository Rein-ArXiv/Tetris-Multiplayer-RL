#pragma once
#include "simulation/catalog.h"
#include "simulation/collision.h"
#include "simulation/rotation_math.h"
#include <algorithm>
#include <optional>

namespace study_rotation {
enum class Result { rotated, blocked, invalid };

inline std::optional<study_rotation_math::PivotTwice> pivot(study_catalog::Kind kind) noexcept {
    if (!study_catalog::find(kind)) return std::nullopt;
    if (kind == study_catalog::Kind::I) return study_rotation_math::PivotTwice{3,3};
    if (kind == study_catalog::Kind::O) return study_rotation_math::PivotTwice{1,1};
    return study_rotation_math::PivotTwice{2,2};
}

// Derive a canonical row-major cell list from immutable spawn data.
inline std::optional<study_piece::Shape> shape_at(study_catalog::Kind kind,
                                                int quarter) noexcept {
    if (quarter < 0 || quarter > 3) return std::nullopt;
    const auto* definition = study_catalog::find(kind);
    const auto center = pivot(kind);
    if (!definition || !center) return std::nullopt;
    auto shape = definition->cells;
    for (int i=0;i<quarter;++i) {
        const auto next = study_rotation_math::clockwise(shape,*center);
        if (!next) return std::nullopt;
        shape = *next;
    }
    return shape;
}

inline bool same_cells(study_piece::Shape a, study_piece::Shape b) noexcept {
    const auto less = [](const auto& x,const auto& y) {
        return x.row < y.row || (x.row == y.row && x.column < y.column);
    };
    std::sort(a.begin(),a.end(),less); std::sort(b.begin(),b.end(),less);
    for (std::size_t i=0;i<a.size();++i)
        if(a[i].row!=b[i].row || a[i].column!=b[i].column) return false;
    return true;
}

// Piece.local is cached shape data. Require it to agree with kind/quarter;
// the origin is independent. Commit both cells and phase only on success.
inline Result try_clockwise(const study_grid::Grid& board,study_piece::Piece& piece,
                            study_catalog::Kind kind,int& quarter) noexcept {
    const auto expected = shape_at(kind,quarter);
    if (!expected || !same_cells(piece.local,*expected) ||
        study_collision::classify(board,piece)!=study_collision::Placement::clear)
        return Result::invalid;
    const int next_quarter = (quarter+1)%4;
    const auto next = shape_at(kind,next_quarter);
    if (!next) return Result::invalid;
    auto candidate = piece;
    candidate.local = *next;
    const auto placement = study_collision::classify(board,candidate);
    if (placement == study_collision::Placement::outside ||
        placement == study_collision::Placement::occupied) return Result::blocked;
    if (placement != study_collision::Placement::clear) return Result::invalid;
    piece = candidate;
    quarter = next_quarter;
    return Result::rotated;
}
} // namespace study_rotation
