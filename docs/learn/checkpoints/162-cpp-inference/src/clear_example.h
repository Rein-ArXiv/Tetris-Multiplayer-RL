#pragma once
#include "simulation/catalog.h"
#include "simulation/grid.h"

// A prepared exercise: one almost-full bottom row, two for O.
// Markers outside the spawn columns reveal how surviving rows move.
inline study_grid::Grid make_clear_board(study_catalog::Kind kind) {
    study_grid::Grid board;
    const auto piece = study_catalog::make_piece(kind);
    if (!piece) return board;
    const int first = kind == study_catalog::Kind::O ? 18 : 19;
    for (int row = first; row < 20; ++row) {
        for (int column = 0; column < 10; ++column) {
            bool hole = false;
            for (const auto local : piece->local) {
                if (18 + local.row == row && piece->origin.column + local.column == column)
                    hole = true;
            }
            if (!hole) (void)board.set(row,column,study_grid::Cell::filled);
        }
    }
    (void)board.set(5,0,study_grid::Cell::filled);
    (void)board.set(12,9,study_grid::Cell::filled);
    return board;
}
