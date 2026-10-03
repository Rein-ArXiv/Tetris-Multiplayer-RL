#ifndef STUDY_GRID_GRID_H_
#define STUDY_GRID_GRID_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace study_grid {

enum class Cell : std::uint8_t {
  empty = 0,
  filled = 1,
};

struct Position {
  int row = 0;
  int column = 0;
};

class Grid {
 public:
  static constexpr int kRows = 20;
  static constexpr int kColumns = 10;
  static constexpr std::size_t kCount =
      static_cast<std::size_t>(kRows) * static_cast<std::size_t>(kColumns);

  // True only when both coordinates lie inside the board. Each coordinate
  // is checked independently so no arithmetic can overflow first.
  static constexpr bool contains(int row, int column) noexcept {
    return row >= 0 && row < kRows && column >= 0 && column < kColumns;
  }

  // Row-major index, or nullopt for any out-of-range coordinate. Bounds are
  // validated before conversion or multiplication, so (0, 10) is rejected
  // instead of aliasing (1, 0).
  static constexpr std::optional<std::size_t> index_of(int row,
                                                       int column) noexcept {
    if (!contains(row, column)) {
      return std::nullopt;
    }
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(kColumns) +
           static_cast<std::size_t>(column);
  }

  // Inverse of index_of: the position for a valid flat index, else nullopt.
  // The index is range-checked before the division, and the quotient and
  // remainder are small enough to cast back to int safely.
  static constexpr std::optional<Position> position_of(
      std::size_t index) noexcept {
    if (index >= kCount) {
      return std::nullopt;
    }
    const std::size_t row = index / static_cast<std::size_t>(kColumns);
    const std::size_t column = index % static_cast<std::size_t>(kColumns);
    return Position{static_cast<int>(row), static_cast<int>(column)};
  }

  // The cell at (row, column), or nullopt when the coordinate is outside the
  // board. A stored Cell::empty is a present value, not an absent one.
  constexpr std::optional<Cell> get(int row, int column) const noexcept {
    const std::optional<std::size_t> index = index_of(row, column);
    if (!index.has_value()) {
      return std::nullopt;
    }
    return cells_[*index];
  }

  // Writes one cell. Returns false and leaves the grid unchanged when either
  // the coordinate or the value is invalid.
  constexpr bool set(int row, int column, Cell value) noexcept {
    if (!is_valid_cell(value)) {
      return false;
    }
    const std::optional<std::size_t> index = index_of(row, column);
    if (!index.has_value()) {
      return false;
    }
    cells_[*index] = value;
    return true;
  }

  // False outside the board; otherwise true only for an actual empty cell.
  constexpr bool is_empty(int row, int column) const noexcept {
    const std::optional<Cell> cell = get(row, column);
    return cell.has_value() && *cell == Cell::empty;
  }

  // Resets every cell to Cell::empty. std::array::fill is not constexpr before
  // C++20, so this is deliberately not marked constexpr.
  void clear() noexcept { cells_.fill(Cell::empty); }

  // Read-only borrowed view of the row-major storage. Valid only while this
  // grid is alive; copy the array when an independent owner is needed.
  constexpr const std::array<Cell, kCount>& cells() const noexcept {
    return cells_;
  }

 private:
  static constexpr bool is_valid_cell(Cell value) noexcept {
    return value == Cell::empty || value == Cell::filled;
  }

  std::array<Cell, kCount> cells_{};
};

}  // namespace study_grid

#endif  // STUDY_GRID_GRID_H_