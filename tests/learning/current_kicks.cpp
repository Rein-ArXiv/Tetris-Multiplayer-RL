#include "src/sim_game.h"
#include "simulation/kicks.h"
#include <cstdio>
int main(){
    for(unsigned seed=1;seed<1000;++seed){
        SimGame game(seed);if(game.CurrentBlockId()!=6)continue;
        game.SubmitInput(INPUT_ROTATE);
        for(int i=0;i<10;++i)game.SubmitInput(INPUT_LEFT);
        if(game.CurrentRotation()!=1)return 1;
        const auto actual=game.CurrentBlock().GetCellPositions();
        auto local=*study_rotation::shape_at(study_catalog::Kind::T,1);
        study_piece::Piece piece{local,{actual[0].row-local[0].row,actual[0].column-local[0].column}};
        if(piece.origin.column!=-1)return 1;
        auto before=game.StateHash();game.SubmitInput(INPUT_ROTATE);
        if(game.CurrentRotation()!=1||game.StateHash()!=before)return 1;
        study_grid::Grid empty;int q=1;
        auto result=study_kicks::try_clockwise(empty,piece,study_catalog::Kind::T,q);
        if(result.result!=study_rotation::Result::rotated||result.candidate_index!=1||q!=2||piece.origin.column!=0)return 1;
        std::puts("Production rejects T at left wall; teaching kick accepts candidate1 and moves origin -1 -> 0: intentional rule difference");return 0;
    }
    return 1;
}
