#include "piece_example.h"
#include "board_example.h"
#include "renderer/piece_geometry.h"
#include <cstdio>
#include <climits>
int main(){
    const auto board=make_example_board();
    const auto before=board.cells();
    auto piece=make_example_piece();
    const auto bounds=study_piece::local_bounds(piece.local);
    std::printf("local bounds: rows %d..%d columns %d..%d\n",
                bounds.min_row,bounds.max_row,bounds.min_column,bounds.max_column);
    for(const study_piece::Origin origin:{study_piece::Origin{4,3},{-1,3},{-4,3},{19,8},{INT_MAX,3}}){
        piece.origin=origin;
        const auto cells=study_piece::to_board(piece);
        std::printf("origin=(%d,%d):",origin.row,origin.column);
        if(!cells){std::puts(" arithmetic failure");continue;}
        for(const auto p:*cells)std::printf(" (%d,%d)",p.row,p.column);
        const auto mesh=study_piece_view::make_visible_mesh(*cells);
        std::printf(" visible=%zu vertices=%zu\n",mesh.count/6,mesh.count);
    }
    return board.cells()==before?0:1;
}
