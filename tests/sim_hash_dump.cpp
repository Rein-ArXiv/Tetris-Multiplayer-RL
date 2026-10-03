// Renderer-free observation emitter for a fixed legacy-hash regression script.
// The output is a candidate capture, not an independent oracle of rule correctness.
// Compare like-for-like rules, seed/input script, hash format and observation boundaries.
// A mismatch may arise from rules, encoding, inputs, build differences or a regression.
// Cross-platform claims require observations from each actual target build.
// Capture a separate file for review; do not overwrite the committed reference on failure.
// Usage: sim_hash_dump > candidate-sim-hash.txt

#include <cstdint>
#include <cstdio>
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "../src/sim_game.h"
#include "../core/input.h"
#include "../core/constants.h"

namespace {

// Preserve base-0 seed notation: decimal, leading-zero octal, or 0x hexadecimal.
// Reject signs, whitespace, trailing characters and uint64 overflow before output.
std::optional<uint64_t> parse_seed(std::string_view text)
{
    if (text.empty() || text.front() == '+' || text.front() == '-') return std::nullopt;
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    } else if (text.size() > 1 && text[0] == '0') {
        base = 8;
        text.remove_prefix(1);
    }
    uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

struct Step
{
    // For each step we submit an input mask, then advance N ticks.
    // The mask is submitted unconditionally — INPUT_NONE (0) is a valid mask
    // and the sim treats it as a no-op, so "no input this step" is just 0.
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
// This finite scenario samples those actions; it does not cover every rule branch.
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
            const auto seed = parse_seed(argv[i]);
            if (!seed) {
                std::fprintf(stderr, "invalid seed: use unsigned decimal, octal or 0x hexadecimal uint64\n");
                return 2;
            }
            seeds.push_back(*seed);
        }
    }

    for (uint64_t seed : seeds)
    {
        std::printf("==== seed 0x%016llx ====\n",
                    static_cast<unsigned long long>(seed));
        run_and_dump(seed);
        std::printf("\n");
    }

    return std::fflush(stdout) == 0 && !std::ferror(stdout) ? 0 : 1;
}
