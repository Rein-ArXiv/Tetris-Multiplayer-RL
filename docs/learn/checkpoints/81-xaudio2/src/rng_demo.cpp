#include "core/rng.h"
#include "simulation/round.h"
#include "src/seed_option.h"
#include <cinttypes>
#include <cstdio>
int main(int argc, char** argv) {
    const auto seed = study_seed::parse(argc == 2 ? argv[1] : "1");
    if (argc > 2 || !seed) { std::fputs("Usage: rng_demo [decimal uint64 seed]\n", stderr); return 2; }
    XorShift64Star engine(*seed);
    std::printf("engine initial=%" PRIu64 "\n", engine.getState());
    for (int i = 0; i < 3; ++i) {
        const auto value = engine.next();
        std::printf("next state=%" PRIu64 " output=%" PRIu64 "\n", engine.getState(), value);
    }
    study_next::SeededBagSource supply(*seed);
    std::printf("bag initial=%" PRIu64 " pieces=", supply.rng_state());
    for (int i = 0; i < 14; ++i) {
        if (i == 7) std::putchar('|');
        std::printf("%.*s", 1, study_catalog::find(supply.next())->name.data());
    }
    std::printf(" remaining=%zu cursor=%zu\n", supply.bag().remaining(), supply.cursor());
    auto round = study_round::Round::create(study_grid::Grid{}, study_next::SeededBagSource(*seed));
    if (!round) return 1;
    for (int locks = 0; locks <= 3; ++locks) {
        std::printf("locks=%d current=%.*s preview=", locks, 1, study_catalog::find(round->kind())->name.data());
        for (std::size_t i = 0; i < 3; ++i)
            std::printf("%.*s", 1, study_catalog::find(*round->next().peek(i))->name.data());
        const auto* source = round->source().seeded();
        std::printf(" remaining=%zu state=%" PRIu64 "\n", source->bag().remaining(), source->rng_state());
        if (locks < 3) (void)round->tick(0, false, false, true);
    }
}
