#include "src/sim_game.h"
#include "simulation/catalog.h"
#include "simulation/gravity.h"
#include <cstdio>
int main() {
    unsigned checks=0;bool seen[8]{};
    for(unsigned seed=1;seed<=100;++seed) {
        SimGame game(seed);const int id=game.CurrentBlockId();seen[id]=true;
        auto piece=study_catalog::make_piece(static_cast<study_catalog::Kind>(id));
        if(!piece)return 1;
        study_grid::Grid board;study_gravity::Counter counter;
        for(int tick=1;tick<=510;++tick) { // Empty board; stop before the first lock.
            game.Tick();const auto result=study_gravity::tick(board,*piece,counter);
            if(result==study_gravity::TickResult::invalid || game.CurrentBlockId()!=id ||
               game.CurrentRow()!=piece->origin.row || game.CurrentCol()!=piece->origin.column)return 1;
            for(const auto& row:game.Grid())for(int cell:row)if(cell)return 1;
            ++checks;
        }
    }
    for(int id=1;id<=7;++id)if(!seen[id])return 1;
    std::printf("%u pre-lock ticks compared with actual SimGame::Tick across all seven kinds\n",checks);
}
