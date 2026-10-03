#include "src/kicks_example.h"
#include <cstdio>
int main(){
    for(int n=0;n<7;++n){
        const auto c=kicks_example::make(n);
        study_piece::Piece piece{*study_rotation::shape_at(c.kind,c.quarter),c.origin};int quarter=c.quarter;
        auto outcome=study_kicks::try_clockwise(c.board,piece,c.kind,quarter);
        std::printf("case=%d %s candidate=%d quarter=%d origin=(%d,%d)\n",n,
            outcome.result==study_rotation::Result::rotated?"rotated":"blocked",
            outcome.candidate_index,quarter,piece.origin.row,piece.origin.column);
        if(outcome.result==study_rotation::Result::invalid)return 1;
    }
}
