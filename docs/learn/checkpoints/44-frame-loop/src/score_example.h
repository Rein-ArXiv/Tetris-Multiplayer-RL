#pragma once
#include "simulation/grid.h"
// Four successive vertical I placements each complete four rows.
inline study_grid::Grid make_four_clears() noexcept {
    study_grid::Grid board;
    for (int row=4; row<20; ++row)
        for (int column=0; column<10; ++column)
            if (column!=5) (void)board.set(row,column,study_grid::Cell::filled);
    return board;
}
