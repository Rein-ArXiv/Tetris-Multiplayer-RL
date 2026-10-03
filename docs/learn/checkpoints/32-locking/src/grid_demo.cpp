#include "board_example.h"
#include <cstdio>

int main() {
    using study_grid::Grid;
    using study_grid::Cell;
    const Grid board=make_example_board();
    std::printf("board %d rows x %d columns; %zu cells; %zu bytes per cell\n",
                Grid::kRows,Grid::kColumns,Grid::kCount,sizeof(Cell));
    for(int row=0; row<Grid::kRows; ++row) {
        std::printf("%02d ",row);
        for(int column=0; column<Grid::kColumns; ++column) {
            const auto cell=board.get(row,column);
            if(!cell)return 1; // Every loop coordinate must be valid.
            std::putchar(*cell==Cell::empty ? '.' : '#');
        }
        std::putchar('\n');
    }
    const auto index=Grid::index_of(1,2);
    const auto position=index ? Grid::position_of(*index) : std::nullopt;
    if(!position)return 1;
    std::printf("(1,2) -> index %zu -> (%d,%d)\n",*index,position->row,position->column);
    std::printf("get(0,0): present=%d; get(0,1): present=%d; get(0,10): present=%d\n",
                board.get(0,0).has_value(),board.get(0,1).has_value(),board.get(0,10).has_value());
    Grid copy=board;
    if(!copy.set(1,2,Cell::empty))return 1;
    std::printf("after editing copy: original empty=%d, copy empty=%d\n",
                board.is_empty(1,2),copy.is_empty(1,2));
}
