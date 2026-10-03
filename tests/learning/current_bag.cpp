#include "src/sim_game.h"
#include "simulation/seven_bag.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed at %d: %s\n", __LINE__, #x); std::exit(1); } } while (false)
int main() {
    for (std::uint64_t seed = 0; seed < 1000; ++seed) {
        SimGame game(seed);
        XorShift64Star rng(seed ? seed : 0xC0FFEE123456789ull);
        study_bag::SevenBag bag;
        const auto draw = [&] { return static_cast<int>(*bag.take(rng.nextUInt(static_cast<std::uint32_t>(bag.next_bound())))); };
        CHECK(game.CurrentBlockId() == draw());
        CHECK(game.NextBlocks().size() == 3);
        for (const auto& block : game.NextBlocks()) CHECK(block.id == draw());
        CHECK(game.RngState() == rng.getState());
        const auto state = game.RngState();
        for (int read = 0; read < 10; ++read) { (void)game.NextBlock(); (void)game.NextBlocks(); }
        CHECK(game.RngState() == state);
    }
    std::puts("1000 production SimGame constructors match catalog/stable erase/seed fallback and four piece RNG draws; preview reads preserve RNG state");
}
