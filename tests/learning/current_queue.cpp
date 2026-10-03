#include "src/sim_game.h"
#include <cstdio>
int main(){
    for(unsigned seed=1;seed<=100;++seed){
        SimGame game(seed);const auto preview=game.NextBlocks();const SimBlock next=game.NextBlock();
        if(preview.size()!=3||next.id!=preview[0].id)return 1;
        const auto hash=game.StateHash();
        for(int i=0;i<20;++i)if(game.NextBlock().id!=next.id||game.NextBlocks().size()!=3||game.StateHash()!=hash)return 1;
        game.SubmitInput(INPUT_DROP);
        if(game.CurrentBlockId()!=next.id||game.NextBlocks().size()!=3||
           game.NextBlocks()[0].id!=preview[1].id||game.NextBlocks()[1].id!=preview[2].id)return 1;
        const auto current_before=game.CurrentBlock().GetCellPositions();
        if(current_before.size()!=4||game.CurrentRotation()!=0)return 1;
        game.SubmitInput(INPUT_RIGHT);
        if(game.NextBlocks()[0].id!=preview[1].id||game.NextBlocks()[0].rotationState!=0)return 1;
        if(next.id!=preview[0].id||next.rotationState!=0)return 1;
    }
    std::puts("100 production seeds: preview reads preserve hash, first hard drop promotes value then shifts/refills, copied snapshots stay independent");
}
