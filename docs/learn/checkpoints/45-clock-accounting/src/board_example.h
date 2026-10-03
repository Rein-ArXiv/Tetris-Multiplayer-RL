#pragma once
#include "simulation/grid.h"

// Application sample data, not part of Grid's storage contract.
inline study_grid::Grid make_example_board() noexcept {
    study_grid::Grid board;
    // All three positions are inside the fixed 20x10 board.
    (void)board.set(0,0,study_grid::Cell::filled);
    (void)board.set(1,2,study_grid::Cell::filled);
    (void)board.set(study_grid::Grid::kRows-1,study_grid::Grid::kColumns-1,
                    study_grid::Cell::filled);
    return board;
}
