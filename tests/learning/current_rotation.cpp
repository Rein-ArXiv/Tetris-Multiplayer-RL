#include "src/sim_game.h"
#include "simulation/rotation.h"
#include <array>
#include <cstdio>
int main() {
    std::array<SimBlock,7> blocks{SimIBlock(),SimJBlock(),SimLBlock(),SimOBlock(),SimSBlock(),SimTBlock(),SimZBlock()};
    for(auto& block:blocks)for(int q=0;q<4;++q) {
        const auto expected=study_rotation::shape_at(static_cast<study_catalog::Kind>(block.id),q);
        if(!expected||block.rotationState!=q)return 1;
        const auto& cells=block.cells.at(q);
        if(cells.size()!=4)return 1;
        for(int i=0;i<4;++i)if(cells[i].row!=(*expected)[i].row||cells[i].column!=(*expected)[i].column)return 1;
        block.Rotate();
    }
    for(unsigned seed=1;seed<=100;++seed) {
        SimGame game(seed);study_grid::Grid board;auto kind=static_cast<study_catalog::Kind>(game.CurrentBlockId());
        auto piece=*study_catalog::make_piece(kind);int q=0;
        for(int i=0;i<4;++i) {
            game.SubmitInput(INPUT_ROTATE);
            if(study_rotation::try_clockwise(board,piece,kind,q)!=study_rotation::Result::rotated||q!=game.CurrentRotation())return 1;
            auto cells=study_piece::to_board(piece);auto actual=game.CurrentBlock().GetCellPositions();
            for(int j=0;j<4;++j)if((*cells)[j].row!=actual[j].row||(*cells)[j].column!=actual[j].column)return 1;
        }
    }
    // Fixture: successful S 0->1 then rejected 1->2. Existing rotate history stays.
    for(unsigned seed=1;seed<1000;++seed) {
        SimGame game(seed);if(game.CurrentBlockId()!=5)continue;
        auto& board=const_cast<int(&)[20][10]>(game.Grid());board[2][3]=1;
        game.SubmitInput(INPUT_ROTATE);if(game.CurrentRotation()!=1)return 1;
        const auto before=game.StateHash();
        game.SubmitInput(INPUT_ROTATE);if(game.CurrentRotation()!=1||game.StateHash()!=before)return 1;
        std::puts("28 canonical production shapes and 400 SimGame rotations matched; rejected rotation preserves preceding successful state hash");return 0;
    }
    return 1;
}
