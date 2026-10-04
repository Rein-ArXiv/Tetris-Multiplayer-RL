#include "net/delayed_lockstep.h"
#include <cstdio>
int main() {
    using namespace study_net;
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},77);
    if(!round)return 1;
    const study_combat::Duel duel(*round,*round);
    auto immediate=DelayedLockstep::create(duel,Side::host,0);
    auto delayed=DelayedLockstep::create(duel,Side::host,2);
    if(!immediate||!delayed)return 1;
    const unsigned arrived[]={0,1,2,2,2,5,6,7};
    unsigned received=0;
    std::puts("pulse remote_max next_D0 next_D2");
    for(unsigned pulse=0;pulse<8;++pulse) {
        if(immediate->capture(0)!=Put::stored || delayed->capture(0)!=Put::stored)return 1;
        while(received<=arrived[pulse]) {
            if(immediate->receive(received,0)!=Put::stored || delayed->receive(received,0)!=Put::stored)return 1;
            ++received;
        }
        for(auto* game:{&*immediate,&*delayed}) {
            Advance result;
            do {result=game->advance();} while(result==Advance::advanced);
            if(result!=Advance::waiting)return 1;
        }
        std::printf("%u %u %llu %llu\n",pulse,arrived[pulse],
            static_cast<unsigned long long>(immediate->next_tick()),
            static_cast<unsigned long long>(delayed->next_tick()));
    }
}
