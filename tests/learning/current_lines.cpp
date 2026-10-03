#include "src/sim_game.h"
#include "simulation/lines.h"
#include "simulation/round.h"
#include <array>
#include <cstdio>
int main() {
    unsigned state=11;
    for(int trial=0;trial<20000;++trial) {
        SimGrid current;study_grid::Grid teaching;
        std::array<std::array<int,10>,20> kept{};int survivors=0;
        for(int r=0;r<20;++r) {
            state=state*1664525u+1013904223u;bool full=state%4==0;
            bool actual_full=true;
            for(int c=0;c<10;++c) {
                state=state*1664525u+1013904223u;
                // Production IDs are preserved, including nonzero legacy8.
                const int value=full?static_cast<int>(state%9+1):static_cast<int>(state%4==0?0:state%9+1);
                current.grid[r][c]=value;actual_full=actual_full&&value!=0;
                if(value)(void)teaching.set(r,c,study_grid::Cell::filled);
            }
            if(!actual_full) { for(int c=0;c<10;++c)kept[survivors][c]=current.grid[r][c];++survivors; }
        }
        int removed=current.ClearFullRows();if(removed!=20-survivors||study_lines::clear_full_rows(teaching)!=removed)return 1;
        for(int r=0;r<20;++r)for(int c=0;c<10;++c) {
            int expected=r<removed?0:kept[r-removed][c];
            if(current.grid[r][c]!=expected||(!teaching.is_empty(r,c))!=(expected!=0))return 1;
        }
    }
    // Controlled fixture only: underlying game is non-const. Populate through
    // the public read view solely to exercise existing production transition.
    for(unsigned seed=1;seed<1000;++seed) {
        SimGame game(seed);if(game.CurrentBlockId()!=3)continue;
        auto& fixture=const_cast<int(&)[20][10]>(game.Grid());
        study_grid::Grid teaching;
        for(int c=0;c<10;++c)if(c<3||c>6) {fixture[1][c]=1;(void)teaching.set(1,c,study_grid::Cell::filled);}
        fixture[2][3]=1;(void)teaching.set(2,3,study_grid::Cell::filled);
        auto round=study_round::Round::create(teaching,study_catalog::Kind::I,1);
        game.MoveBlockDown();
        if(game.lastLinesCleared!=1||!game.IsGameOver())return 1;
        for(const auto cell:game.CurrentBlock().GetCellPositions())if(game.Grid()[cell.row][cell.column]!=0)return 1;
        if(round->tick(0)!=study_round::Step::locked||round->last_cleared()!=1||round->finished())return 1;
        std::puts("20000 production ID-preserving boards matched; production pre-clear top-out persists although post-clear spawn is empty, teaching clear-first continues");
        return 0;
    }
    return 1;
}
