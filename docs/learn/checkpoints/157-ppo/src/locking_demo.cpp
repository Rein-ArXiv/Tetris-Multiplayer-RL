#include "simulation/round.h"
#include "src/gravity_example.h"
#include <cstdio>
int main() {
    auto round=study_round::Round::create(make_gravity_board(),study_catalog::Kind::T);
    if(!round)return 1;
    int locks=0;
    for(int tick=1;!round->finished() && tick<=1000;++tick) {
        const auto result=round->tick(0);
        if(result==study_round::Step::invalid)return 1;
        if(result==study_round::Step::locked||result==study_round::Step::game_over) {
            ++locks;unsigned filled=0;
            for(const auto cell:round->board().cells())if(cell==study_grid::Cell::filled)++filled;
            std::printf("tick=%d locks=%d filled=%u active=%s\n",tick,locks,filled,
                        round->active()?"spawned":"none (game over)");
        }
    }
    return round->finished()?0:1;
}
