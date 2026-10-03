#ifndef STUDY_LINES_LINES_H_
#define STUDY_LINES_LINES_H_

#include <optional>

#include "simulation/grid.h"

namespace study_lines {

// Returns true only when every column of `row` holds Cell::filled. A row index
// outside [0, study_grid::Grid::kRows) is never full and returns false. The
// grid is borrowed read-only, so this has no side effects.
inline bool row_full(const study_grid::Grid& grid, int row) noexcept {
  if (row < 0 || row >= study_grid::Grid::kRows) {
    return false;
  }
  for (int column = 0; column < study_grid::Grid::kColumns; ++column) {
    const std::optional<study_grid::Cell> cell = grid.get(row, column);
    if (!cell.has_value() || *cell != study_grid::Cell::filled) {
      return false;
    }
  }
  return true;
}

// Removes every stored full row from `grid` and compacts the surviving rows
// toward the bottom, preserving their relative order. Returns the number of
// rows removed, in [0, study_grid::Grid::kRows].
//
// The grid must hold exactly study_grid::Grid::kRows (20) by
// study_grid::Grid::kColumns (10) cells. Every coordinate used below lies
// inside those bounds, so each study_grid::Grid::get and
// study_grid::Grid::set call in this function is guaranteed to succeed; no
// runtime failure path is being skipped here.
//
// Both indices start at the bottom row (19). Decreasing an index moves
// upward on this board. `write` advances only for survivors, while `read`
// advances every iteration: write >= read at each iteration's start.
// Writes therefore affect only the current row or rows already examined,
// never an unread row above `read`. Do not clear a source immediately.
inline int clear_full_rows(study_grid::Grid& grid) noexcept {
  int write = study_grid::Grid::kRows - 1;
  int removed = 0;
  for (int read = study_grid::Grid::kRows - 1; read >= 0; --read) {
    if (row_full(grid, read)) {
      ++removed;
      continue;
    }
    if (write != read) {
      for (int column = 0; column < study_grid::Grid::kColumns; ++column) {
        const std::optional<study_grid::Cell> cell = grid.get(read, column);
        (void)grid.set(write, column, *cell);
      }
    }
    --write;
  }
  // Rows 0..write still describe the vacated stack space above the surviving
  // rows, so reset them to empty. When nothing was removed, `write` is -1 and
  // this loop does not run.
  for (int row = 0; row <= write; ++row) {
    for (int column = 0; column < study_grid::Grid::kColumns; ++column) {
      (void)grid.set(row, column, study_grid::Cell::empty);
    }
  }
  return removed;
}

}  // namespace study_lines

#endif  // STUDY_LINES_LINES_H_
