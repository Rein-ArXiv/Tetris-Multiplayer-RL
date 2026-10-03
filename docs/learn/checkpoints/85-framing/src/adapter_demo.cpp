#include "client/game.h"
#include "simulation/state_hash.h"
#include <cinttypes>
#include <cstdio>
int main() {
    auto initial = study_round::Round::create_seeded(study_grid::Grid{}, 1);
    if (!initial) return 1;
    study_game::Game game(*initial);
    const auto before = game.view();
    const auto report = game.advance(0.017, {false,false,false,false,true,false});
    const auto after = game.view();
    if (!before || !after || !report || !after->active) return 1;
    std::printf("before active cells=%zu after first next=%u ticks=%u blue=%.5f\n",
        before->active->local.size(), unsigned(*after->next.peek()), report->ticks, game.background().b);
    const auto hash = study_hash::state_hash(game.round());
    for (int i=0; i<100; ++i) if (!game.view()) return 1;
    if (study_hash::state_hash(game.round()) != hash) return 1;
    std::printf("100 reads preserve hash=%016" PRIx64 " accent=%" PRIu64 "\n", *hash, game.accent_state());
}
