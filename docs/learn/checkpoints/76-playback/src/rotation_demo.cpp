#include "simulation/rotation.h"
#include <cstdio>
int main() {
    study_grid::Grid board;
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    piece.origin={6,3};int quarter=0;
    for(int step=0;step<=4;++step) {
        std::printf("quarter=%d cells=",quarter);
        for(auto cell:piece.local)std::printf("(%d,%d)",cell.row,cell.column);
        std::puts("");
        if(step<4 && study_rotation::try_clockwise(board,piece,study_catalog::Kind::T,quarter)!=study_rotation::Result::rotated)return 1;
    }
    piece=*study_catalog::make_piece(study_catalog::Kind::T);piece.origin={18,3};quarter=0;
    if(study_rotation::try_clockwise(board,piece,study_catalog::Kind::T,quarter)!=study_rotation::Result::blocked)return 1;
    std::printf("floor: blocked quarter=%d row=%d\n",quarter,piece.origin.row);
}
