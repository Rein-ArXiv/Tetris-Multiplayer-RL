#ifndef STUDY_ACTIONS_ACTION_SPACE_H
#define STUDY_ACTIONS_ACTION_SPACE_H

#include <climits>
#include <optional>

#include "simulation/grid.h"

namespace study_actions {

inline constexpr int kOrientations = 4;
inline constexpr int kColumns = study_grid::Grid::kColumns;

static_assert(kColumns > 0, "grid must have at least one column");
static_assert(kColumns <= INT_MAX / kOrientations,
              "kColumns * kOrientations would overflow int");

inline constexpr int kCount = kColumns * kOrientations;

// Quarter-turn orientations are labels, not shapes: geometrically duplicate
// shapes keep distinct orientation labels and remain separate actions.
struct Target {
  int column;
  int quarter;
};

inline constexpr std::optional<int> encode(int column, int quarter) noexcept {
  if (column < 0 || quarter < 0) {
    return std::nullopt;
  }
  if (column >= kColumns || quarter >= kOrientations) {
    return std::nullopt;
  }
  return column * kOrientations + quarter;
}

inline constexpr std::optional<Target> decode(int action) noexcept {
  if (action < 0 || action >= kCount) {
    return std::nullopt;
  }
  return Target{action / kOrientations, action % kOrientations};
}

}  // namespace study_actions

#endif  // STUDY_ACTIONS_ACTION_SPACE_H
