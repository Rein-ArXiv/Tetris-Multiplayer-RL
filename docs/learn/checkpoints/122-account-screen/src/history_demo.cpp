#include "simulation/round.h"
#include "src/spin_example.h"
#include <cstdio>
int main() {
    for (int rows=0;rows<=2;++rows) {
        auto r=study_round::Round::create(make_spin_board(rows),study_catalog::Kind::T);
        if (!r || r->tick(0,true,false,true)!=study_round::Step::locked) return 1;
        std::printf("spin=%d score=%llu attack=%llu streak=%llu\n",r->last_t_spin_lines(),
            (unsigned long long)r->score(),(unsigned long long)r->attack_sent(),(unsigned long long)r->clear_streak());
    }
    study_history::Chain chain;
    for(int rows:{1,1,1,0,1}) {
        auto r=study_history::reward(chain,rows,false);if(!r)return 1;chain=r->chain;
        std::printf("rows=%d streak=%llu attack=%d\n",rows,(unsigned long long)chain.clears,r->attack);
    }
}
