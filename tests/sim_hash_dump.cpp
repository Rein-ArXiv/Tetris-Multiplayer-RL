// [NET/RL] Determinism regression test — raylib-free.
//
// Runs SimGame through a FIXED deterministic sequence of inputs and ticks,
// then dumps the state hash at well-known checkpoints. This binary is the
// ground truth for three separate gates:
//
//   1. Refactor parity:        old Game::ComputeStateHash()
//                              == new SimGame::StateHash()
//      (run the same input sequence with both versions and diff the output)
//
//   2. Cross-platform parity:  Linux (Colab) build
//                              == Windows (local) build
//      (XorShift64* + FNV-1a are pure integer ops, but int widths and
//       unsigned modulo behaviour are worth verifying explicitly)
//
//   3. CI regression:          any code change that is *supposed* to preserve
//      behaviour should not change this dump. If the hash moves, the change
//      touched sim semantics and every lockstep peer will desync.
//
// Usage:
//   sim_hash_dump              -> prints hash checkpoints to stdout
//   sim_hash_dump > out.txt    -> capture for diff
//
// The test does NOT depend on net/, raylib, or any I/O other than stdout.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../src/sim_game.h"
#include "../core/input.h"
#include "../core/constants.h"

namespace {

struct Step
{
    // For each step we (optionally) submit an input mask, then advance N ticks.
    // mask == 0xFF means "no input this step" (0 is a valid NONE mask that we
    // still want to be able to submit explicitly).
    uint8_t mask;
    int     ticks;
};

// A deterministic scripted sequence. Chosen to exercise:
//   - horizontal movement in both directions
//   - rotation (valid + wall-rejected)
//   - soft drop and hard drop
//   - multi-tick gravity without input
//   - block locks and bag refills
//
// Length is modest (~60 placements worth) but every branch is hit.
const Step kScript[] = {
    { INPUT_NONE,                           30 },
    { INPUT_LEFT,                            1 },
    { INPUT_LEFT,                            1 },
    { INPUT_LEFT,                            1 },
    { INPUT_ROTATE,                          1 },
    { INPUT_DROP,                            2 },
    { INPUT_NONE,                            5 },
    { INPUT_RIGHT,                           1 },
    { INPUT_RIGHT,                           1 },
    { INPUT_ROTATE,                          1 },
    { INPUT_ROTATE,                          1 },
    { INPUT_DOWN,                            1 },
    { INPUT_DOWN,                            1 },
    { INPUT_DROP,                            2 },
    { INPUT_NONE,                           10 },
    { INPUT_LEFT,                            1 },
    { INPUT_DROP,                            1 },
    { INPUT_NONE,                            5 },
    { INPUT_RIGHT,                           1 },
    { INPUT_RIGHT,                           1 },
    { INPUT_RIGHT,                           1 },
    { INPUT_RIGHT,                           1 },
    { INPUT_ROTATE,                          1 },
    { INPUT_DROP,                            1 },
    { INPUT_NONE,                          120 },
    { INPUT_LEFT  | INPUT_ROTATE,            1 },
    { INPUT_DROP,                            1 },
    { INPUT_NONE,                           30 },
    { INPUT_RIGHT | INPUT_ROTATE,            1 },
    { INPUT_DROP,                            1 },
    { INPUT_NONE,                          180 },
};

void run_and_dump(uint64_t seed)
{
    SimGame sim(seed);
    std::printf("seed=0x%016llx\n", static_cast<unsigned long long>(seed));
    std::printf("initial_hash=0x%016llx\n",
                static_cast<unsigned long long>(sim.StateHash()));

    int step_index = 0;
    int total_ticks = 0;
    for (const Step& step : kScript)
    {
        // INPUT_NONE == 0 is the "no input" sentinel and is fine to submit
        // explicitly — the sim treats an empty mask as a no-op.
        sim.SubmitInput(step.mask);
        for (int i = 0; i < step.ticks; ++i)
        {
            sim.Tick();
            ++total_ticks;
        }
        std::printf(
            "step=%03d mask=0x%02x ticks=%d total_ticks=%d "
            "score=%d over=%d hash=0x%016llx\n",
            step_index,
            static_cast<unsigned>(step.mask),
            step.ticks,
            total_ticks,
            sim.Score(),
            sim.IsGameOver() ? 1 : 0,
            static_cast<unsigned long long>(sim.StateHash()));
        ++step_index;

        if (sim.IsGameOver())
        {
            std::printf("game_over_at_step=%d\n", step_index);
            break;
        }
    }

    std::printf("final_hash=0x%016llx final_score=%d final_over=%d\n",
                static_cast<unsigned long long>(sim.StateHash()),
                sim.Score(),
                sim.IsGameOver() ? 1 : 0);
}

} // namespace

int main(int argc, char** argv)
{
    // Default seeds — small fixed set so the dump covers a few RNG trajectories.
    std::vector<uint64_t> seeds = {
        0x0000000000000001ull,
        0x00000000DEADBEEFull,
        0xC0FFEE123456789ull,
    };

    // Allow overriding the seed list from argv for CI or bisection use.
    if (argc > 1)
    {
        seeds.clear();
        for (int i = 1; i < argc; ++i)
        {
            seeds.push_back(static_cast<uint64_t>(std::strtoull(argv[i], nullptr, 0)));
        }
    }

    for (uint64_t seed : seeds)
    {
        std::printf("==== seed 0x%016llx ====\n",
                    static_cast<unsigned long long>(seed));
        run_and_dump(seed);
        std::printf("\n");
    }

    return 0;
}
