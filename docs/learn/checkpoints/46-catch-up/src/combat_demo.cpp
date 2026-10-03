#include "simulation/duel.h"
#include "src/clear_example.h"
#include <cstdio>
int main() {
    using K=study_catalog::Kind;
    auto left=study_round::Round::create(make_clear_board(K::O),K::O,30);
    auto right=study_round::Round::create({},K::I,30);
    if(!left||!right)return 1;
    study_combat::Duel duel(*left,*right);
    for(int tick=1;tick<=2;++tick) {
        const auto step=duel.tick({0,false,false,tick==1},{0,false,false,true});
        if(!step)return 1;
        std::printf("tick=%d sentL=%llu sentR=%llu pendingR=%d insertedR=%d holeCursorR=%u\n",
            tick,static_cast<unsigned long long>(step->left_attack),
            static_cast<unsigned long long>(step->right_attack),duel.right().pending_garbage(),
            duel.right().last_garbage(),duel.right().hole_cursor());
    }
}
