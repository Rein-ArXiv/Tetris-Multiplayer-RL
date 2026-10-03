#pragma once
#include "simulation/grid.h"
// T quarter1 lands at origin(17,3), pivot(18,4); missing NE corner.
inline study_grid::Grid make_spin_board(int lines=1) {
    study_grid::Grid b;
    (void)b.set(17,3,study_grid::Cell::filled);
    (void)b.set(19,3,study_grid::Cell::filled);
    (void)b.set(19,5,study_grid::Cell::filled);
    if (lines>=1) for (int c=0;c<10;++c) if(c!=4) (void)b.set(19,c,study_grid::Cell::filled);
    if (lines>=2) for (int c=0;c<10;++c) if(c!=4&&c!=5) (void)b.set(18,c,study_grid::Cell::filled);
    return b;
}
