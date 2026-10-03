#ifndef STUDY_LOCKING_LOCKING_H_
#define STUDY_LOCKING_LOCKING_H_

#include <array>
#include <cstddef>
#include <optional>

#include "simulation/gravity.h"

namespace study_locking {

enum class Result {
  locked,
  not_grounded,
  invalid,
};

// Attempts to lock `piece` into `board` at its current position.
// `locked` is returned only after all four cells are written; on every
// other outcome `board` and `piece` are left exactly as they were.
// Single-thread state update; this does not synchronize concurrent readers.

inline Result try_lock(study_grid::Grid& board,
                       const study_piece::Piece& piece) noexcept {
  // Map the piece's local cells onto the board; overflow => invalid.
  const std::optional<study_piece::BoardCells> cells =
      study_piece::to_board(piece);
  if (!cells.has_value()) {
    return Result::invalid;
  }

  // The piece must currently sit cleanly: every cell inside and empty.
  if (study_collision::classify(board, piece) !=
      study_collision::Placement::clear) {
    return Result::invalid;
  }

  // Require exactly four distinct board cells; canonical shape and
  // connectedness are deliberately not checked.
  for (std::size_t i = 0; i < cells->size(); ++i) {
    for (std::size_t j = i + 1; j < cells->size(); ++j) {
      if ((*cells)[i].row == (*cells)[j].row &&
          (*cells)[i].column == (*cells)[j].column) {
        return Result::invalid;
      }
    }
  }

  // A piece that can still fall is not grounded and must not lock.
  study_piece::Piece below = piece;
  const study_movement::Result drop = study_gravity::try_down(board, below);
  if (drop == study_movement::Result::moved) {
    return Result::not_grounded;
  }
  if (drop != study_movement::Result::blocked) {
    return Result::invalid;
  }

  // Stage the four writes on a copy; commit only if every write succeeds.
  study_grid::Grid staged = board;
  for (const study_grid::Position& cell : *cells) {
    if (!staged.set(cell.row, cell.column, study_grid::Cell::filled)) {
      return Result::invalid;  // board untouched: only the copy changed
    }
  }

  board = staged;  // single commit after all writes succeeded
  return Result::locked;
}

}  // namespace study_locking

#endif  // STUDY_LOCKING_LOCKING_H_
