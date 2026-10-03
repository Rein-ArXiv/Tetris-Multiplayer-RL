#include "simulation/round.h"
#include "simulation/pending_controls.h"
#include <cstdio>
int main() {
    auto round=study_round::Round::create({},study_catalog::Kind::I,30);
    if(!round)return 1;
    study_input::PendingControls input;
    input.capture(false,false,false,false,true);
    for(int tick=1;tick<=3;++tick) {
        const auto intent=input.consume();
        const auto result=round->tick(intent.horizontal,intent.clockwise,intent.soft_drop,intent.hard_drop);
        if(result==study_round::Step::invalid||!round->active())return 1;
        unsigned filled=0;for(auto cell:round->board().cells())if(cell==study_grid::Cell::filled)++filled;
        std::printf("tick=%d drop=%d distance=%d filled=%u row=%d score=%llu\n",tick,intent.hard_drop,
            round->last_hard_drop_distance(),filled,round->active()->origin.row,
            static_cast<unsigned long long>(round->score()));
    }
}
