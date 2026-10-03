#include "simulation/round.h"
#include "presentation/accent_noise.h"
#include <cinttypes>
#include <cstdio>
static int bottom_hole(const study_grid::Grid& board) {
    for(int c=0;c<10;++c)if(board.get(19,c)==study_grid::Cell::empty)return c;
    return -1;
}
int main() {
    for(unsigned samples : {0u,1u,37u}) {
        auto round=study_round::Round::create_seeded(study_grid::Grid{},1);
        if(!round || !round->add_garbage(1) || !round->add_garbage(2))return 1;
        study_presentation::AccentNoise accent;
        for(unsigned i=0;i<samples;++i)(void)accent.sample();
        const auto step=round->tick(0,false,false,true);
        if(step!=study_round::Step::locked)return 1;
        std::printf("visual=%u current=%.*s hole=%d inserted=%d piece_state=%" PRIu64 " hole_state=%" PRIu64 "\n",
                    samples,1,study_catalog::find(round->kind())->name.data(),bottom_hole(round->board()),
                    round->last_garbage(),round->source().seeded()->rng_state(),*round->hole_source().rng_state());
    }
    auto holes=study_holes::HoleSource::seeded(study_session::garbage_tag);
    std::printf("xor-zero raw=%" PRIu64 " engine=%" PRIu64 "\n",
                study_session::garbage_seed(study_session::garbage_tag),*holes.rng_state());
    // Deliberate shared-engine counterexample: a visual draw consumes the next rule value.
    for(unsigned samples : {0u,1u,37u}) {
        XorShift64Star shared(1);
        for(unsigned i=0;i<samples;++i)(void)shared.nextUInt(5);
        std::printf("shared visual=%u rule_index=%u\n",samples,shared.nextUInt(7));
    }
}
