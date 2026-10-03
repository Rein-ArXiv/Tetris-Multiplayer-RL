#ifndef STUDY_COMBAT_COMBAT_H_
#define STUDY_COMBAT_COMBAT_H_

#include <cstdint>
#include <optional>

#include "simulation/grid.h"  // study_grid::Grid

namespace study_combat {

// Garbage rows sent for a clear count. Only 0..4 are defined; anything
// outside that range is rejected rather than silently clamped.
inline std::optional<int> normal_attack(int cleared) noexcept {
  if (cleared < 0 || cleared > 4) {
    return std::nullopt;
  }
  static constexpr int kTable[5] = {0, 0, 1, 2, 4};
  return kTable[cleared];
}

// Adds `rows` incoming rows to the pending counter, saturating at 20. The sum
// is compared against available headroom before it is formed. The addition
// below runs only when its result is strictly below the cap, even for INT_MAX.
inline std::optional<int> add_pending(int pending, int rows) noexcept {
  constexpr int kCap = study_grid::Grid::kRows;
  if (pending < 0 || pending > kCap || rows < 0) {
    return std::nullopt;
  }
  if (rows >= kCap - pending) {
    return kCap;
  }
  return pending + rows;
}

struct Insertion {
  study_grid::Grid board;
  int rows = 0;
  bool overflow = false;
};

// Pushes `rows` garbage rows in from the bottom. Every existing row r + rows
// moves up to new row r; old rows [0, rows) are discarded and the bottom
// `rows` rows are filled except for `hole`. The input is never mutated: the
// result is assembled in a brand new grid. rows == 0 is a straight copy and
// rows == kRows discards the whole board. Per-cell contents are only
// Cell::empty or Cell::filled; no randomness is used here.
inline std::optional<Insertion> insert(const study_grid::Grid& before, int rows,
                                       int hole) noexcept {
  if (rows < 0 || rows > study_grid::Grid::kRows) {
    return std::nullopt;
  }
  if (hole < 0 || hole >= study_grid::Grid::kColumns) {
    return std::nullopt;
  }

  Insertion result;
  result.rows = rows;

  // Mapped top-to-bottom into the new grid. When r + rows is still inside the
  // board that row is shifted content; otherwise it is a fresh garbage row.
  for (int r = 0; r < study_grid::Grid::kRows; ++r) {
    const int source = r + rows;
    for (int c = 0; c < study_grid::Grid::kColumns; ++c) {
      if (source < study_grid::Grid::kRows) {
        (void)result.board.set(
            r, c, *before.get(source, c));
      } else {
        (void)result.board.set(r, c, c == hole ? study_grid::Cell::empty
                                         : study_grid::Cell::filled);
      }
    }
  }

  // Overflow is reported when any discarded top row actually held a block.
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < study_grid::Grid::kColumns; ++c) {
      if (*before.get(r, c) ==
          study_grid::Cell::filled) {
        result.overflow = true;
      }
    }
  }

  return result;
}

}  // namespace study_combat

#endif  // STUDY_COMBAT_COMBAT_H_
