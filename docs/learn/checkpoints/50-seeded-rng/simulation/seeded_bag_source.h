#ifndef STUDY_NEXT_SEEDED_BAG_SOURCE_H
#define STUDY_NEXT_SEEDED_BAG_SOURCE_H

#include <cstddef>
#include <cstdint>

#include "core/rng.h"
#include "simulation/seven_bag.h"

namespace study_next {

// Game-level seeded piece source: a SevenBag sampled by a XorShift64Star RNG.
// All state is value-owned: no globals, no heap, no reseeding, no draw counter,
// and no clocks. Implicit copies therefore get independent RNG and bag state.
class SeededBagSource {
public:
    using Kind = study_bag::SevenBag::Kind;

    // Match SimGame's zero-seed normalization before constructing the engine.
    static constexpr std::uint64_t default_seed = 0xC0FFEE123456789ull;

    explicit SeededBagSource(std::uint64_t seed) noexcept
        : rng_(seed ? seed : default_seed) {}

    // SevenBag::next_bound() is always in [1, 7], so the reduced index is a valid
    // active draw slot and take() cannot fail; the dereference is guaranteed by
    // that invariant, which is why there is no optional/failure API here. The RNG
    // is advanced on every draw, even when the bound is one, so the call sequence
    // stays deterministic.
    Kind next() noexcept {
        const std::uint32_t bound =
            static_cast<std::uint32_t>(bag_.next_bound());
        const std::size_t index = rng_.nextUInt(bound);
        return *bag_.take(index);
    }

    // Current position inside the active bag, not a lifetime draw counter.
    std::size_t cursor() const noexcept {
        return (study_bag::SevenBag::capacity - bag_.remaining()) % study_bag::SevenBag::capacity;
    }

    // Engine observation only; the remaining bag is also future state.
    std::uint64_t rng_state() const noexcept { return rng_.getState(); }

    // Read-only observation of the bag; no mutation path is exposed.
    const study_bag::SevenBag& bag() const noexcept { return bag_; }

private:
    XorShift64Star rng_;
    study_bag::SevenBag bag_;
};

}  // namespace study_next

#endif  // STUDY_NEXT_SEEDED_BAG_SOURCE_H
