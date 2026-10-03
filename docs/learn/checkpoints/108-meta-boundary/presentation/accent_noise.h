#pragma once

#include <cstdint>

#include "core/rng.h"

namespace study_presentation {

// Owns presentation state; sample mutates only this engine. It receives
// no board, rule engine or clock, so sampling cannot consume rule randomness.
class AccentNoise {
public:
    explicit AccentNoise(std::uint64_t seed = 17ull) noexcept : engine_(seed) {}

    // Returns an accent index in [0, 5); advances only engine_.
    unsigned sample() noexcept {
        return static_cast<unsigned>(engine_.nextUInt(5u));
    }

    // Copy of the current engine state; does not advance the stream.
    std::uint64_t state() const noexcept { return engine_.getState(); }

private:
    XorShift64Star engine_;
};

} // namespace study_presentation
