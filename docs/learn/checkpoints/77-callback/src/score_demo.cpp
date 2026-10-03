#include "simulation/round.h"
#include "src/score_example.h"
#include <cinttypes>
#include <cstdio>
int main(){
    auto round=*study_round::Round::create(make_four_clears(),study_catalog::Kind::I,1);
    for(int lock=0; lock<4; ++lock){
        auto step=round.tick(0,true); // rotate each newly spawned I clockwise
        int ticks=1;
        while(step!=study_round::Step::locked && ticks<1000){
            if(step==study_round::Step::invalid || round.finished())return 1;
            step=round.tick(0);++ticks;
        }
        if(step!=study_round::Step::locked)return 1;
        std::printf("score=%" PRIu64 " gain=%" PRIu64 " lines=%" PRIu64 " level=%u interval=%d\n",
            round.score(),round.last_awarded(),round.total_lines(),round.level(),round.gravity().interval);
    }
}
