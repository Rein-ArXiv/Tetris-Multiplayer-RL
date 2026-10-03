#include "src/sim_game.h"
#include "simulation/round.h"
#include "src/spawn_example.h"
#include <array>
#include <cstdio>
int main(){
    for(unsigned seed=1;seed<=100;++seed)for(bool rescue:{false,true}){
        SimGame game(seed);
        const auto kind=static_cast<study_catalog::Kind>(game.CurrentBlockId());
        const auto next=static_cast<study_catalog::Kind>(game.NextBlock().id);
        const auto board=spawn_example::make(kind,rescue?spawn_example::Scenario::clear_rescue:spawn_example::Scenario::next_blocked);
        if(!board)return 1;
        // Controlled test fixture. The underlying game is not a const object.
        auto& fixture=const_cast<int(&)[20][10]>(game.Grid());
        for(int r=0;r<20;++r)for(int c=0;c<10;++c)fixture[r][c]=board->is_empty(r,c)?0:1;
        std::array<study_catalog::Kind,7> pattern{};pattern[0]=kind;pattern[1]=next;
        const auto source=study_next::ScriptedSource::from_pattern(pattern,2);
        if(!source)return 1;
        auto round=study_round::Round::create(*board,*source,1);if(!round||round->finished())return 1;
        game.MoveBlockDown();const auto result=round->tick(0);
        if(!game.IsGameOver()||!game.gameOverEvent)return 1;
        if(game.lastLinesCleared!=(rescue?(kind==study_catalog::Kind::I?1:2):0))return 1;
        if(result!=(rescue?study_round::Step::locked:study_round::Step::game_over))return 1;
        if(rescue)for(auto cell:game.CurrentBlock().GetCellPositions())if(game.Grid()[cell.row][cell.column]!=0)return 1;
        // Hash is a regression signal, not a proof against hash collisions.
        const auto hash=game.StateHash();const auto id=game.NextBlock().id;
        for(int i=0;i<20;++i){game.SubmitInput(INPUT_DROP|INPUT_RIGHT);game.Tick();if(game.StateHash()!=hash||game.NextBlock().id!=id||!game.IsGameOver())return 1;}
    }
    std::puts("200 production fixtures: pre-clear top-out, clear-first contrast, event and ended input/tick preservation passed");
}
