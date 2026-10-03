#include "src/sim_game.h"
#include "simulation/catalog.h"
#include "simulation/locking.h"
#include <cstdio>
int main() {
    bool seen[8]{};unsigned cases=0;
    for(unsigned seed=1;seed<=100;++seed) {
        SimGame game(seed);const int id=game.CurrentBlockId();seen[id]=true;
        const auto next=game.NextBlock();
        auto piece=*study_catalog::make_piece(static_cast<study_catalog::Kind>(id));
        study_grid::Grid board;
        while(study_gravity::try_down(board,piece)==study_movement::Result::moved)game.MoveBlockDown();
        if(study_locking::try_lock(board,piece)!=study_locking::Result::locked)return 1;
        game.MoveBlockDown(); // Actual first lock, not private-method extraction.
        if(game.IsGameOver()||game.CurrentBlockId()!=next.id||game.CurrentRow()!=next.rowOffset||game.CurrentCol()!=next.columnOffset)return 1;
        for(int r=0;r<20;++r)for(int c=0;c<10;++c) {
            const int expected=board.is_empty(r,c)?0:id;
            if(game.Grid()[r][c]!=expected)return 1;
        }
        ++cases;
    }
    for(int id=1;id<=7;++id)if(!seen[id])return 1;
    std::printf("%u actual first-lock boards and preview promotions compared across all seven kinds\n",cases);
}
