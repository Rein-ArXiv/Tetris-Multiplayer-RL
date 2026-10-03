#include "simulation/round.h"
#include <cstdio>
int main() {
    auto round=study_round::Round::create({},study_catalog::Kind::I,4);
    if(!round)return 1;
    for(int tick=1;tick<=12;++tick){
        const bool held=tick!=4&&tick!=5; // Observe release, then restart at tick6.
        if(round->tick(0,false,held)==study_round::Step::invalid||!round->active())return 1;
        std::printf("tick=%d down=%d row=%d wait=%d gravity=%d\n",tick,held,
            round->active()->origin.row,round->soft_drop().remaining,round->gravity().elapsed);
    }
}
