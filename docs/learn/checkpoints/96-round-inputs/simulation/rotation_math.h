#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

#include "simulation/piece.h"

namespace study_rotation_math {

// Pivot stored doubled: preserves half-integer centers (no rounding).
struct PivotTwice {
    int row = 0;
    int column = 0;
};

// Clockwise quarter turn about a doubled pivot.
// Row grows downward, column right; coordinates are cell-center indices.
//   R2 = pr2 + 2*c - pc2
//   C2 = pc2 - 2*r + pr2
// Every operand is widened to int64 before doubling/add/sub, so the
// intermediate math itself cannot overflow. Each doubled result must be even
// (odd means a non-integral cell center), and each halved value must fit int.
// The candidate is built detached; if any cell fails the whole rotation is
// rejected. No bounds, occupancy, or connectivity checks: pure rotation.
inline std::optional<study_piece::Shape> clockwise(
    const study_piece::Shape& shape, PivotTwice pivot) noexcept {
    study_piece::Shape candidate{};
    for (std::size_t i = 0; i < shape.size(); ++i) {
        const std::int64_t r = static_cast<std::int64_t>(shape[i].row);
        const std::int64_t c = static_cast<std::int64_t>(shape[i].column);
        const std::int64_t pr2 = static_cast<std::int64_t>(pivot.row);
        const std::int64_t pc2 = static_cast<std::int64_t>(pivot.column);

        const std::int64_t row2 = pr2 + 2 * c - pc2;
        const std::int64_t column2 = pc2 - 2 * r + pr2;

        if (row2 % 2 != 0 || column2 % 2 != 0) {
            return std::nullopt;  // non-integral cell center
        }

        const std::int64_t row = row2 / 2;
        const std::int64_t column = column2 / 2;
        if (row < std::numeric_limits<int>::min() ||
            row > std::numeric_limits<int>::max() ||
            column < std::numeric_limits<int>::min() ||
            column > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }

        candidate[i] = study_piece::LocalCell{
            static_cast<int>(row), static_cast<int>(column)};
    }

    // Canonical row-major order, independent of input order.
    std::sort(candidate.begin(), candidate.end(),
              [](const study_piece::LocalCell& a,
                 const study_piece::LocalCell& b) noexcept {
                  if (a.row != b.row) {
                      return a.row < b.row;
                  }
                  return a.column < b.column;
              });
    return candidate;
}

}  // namespace study_rotation_math
