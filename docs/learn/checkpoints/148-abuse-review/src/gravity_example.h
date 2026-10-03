#pragma once
#include "board_example.h"

inline study_grid::Grid make_gravity_board() {
    auto board = make_example_board();
    (void)board.set(10, 4, study_grid::Cell::filled);
    return board;
}
