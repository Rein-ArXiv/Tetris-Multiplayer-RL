#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/tick_inputs.h"
#include "simulation/duel.h"
#include "simulation/input_mask.h"

// Lockstep driver: one simulation tick per (host, peer) input pair at the same
// tick id. Determinism invariant: both processes apply the same canonical
// mapping host->left, peer->right, so the locally observed role never changes
// the simulation. All calls share this owner and must be serialized;
// there are no locks and no wall-clock or countdown input delay.
namespace study_net {

// Result of trying to advance the simulation.
enum class Advance {
    advanced,   // One input pair was applied and the window moved forward.
    waiting,    // At least one side has not submitted this tick yet.
    finished,   // A board already finished; nothing was consumed.
    exhausted,  // The uint32 wire tick space has been fully consumed.
    invalid     // An input failed to decode or the duel rejected the step.
};

class Lockstep {
public:
    explicit Lockstep(const study_combat::Duel& duel, std::uint32_t first = 0) noexcept
        : duel_(duel), inputs_(first) {}

    // Stage input records ahead of time. A missing input is not a neutral zero.
    Put submit(Side side, std::uint32_t tick, unsigned mask) noexcept {
        return inputs_.put(side, tick, mask);
    }

    Put submit_batch(Side side, std::uint32_t first, const std::uint8_t* masks,
                     std::size_t count) noexcept {
        return inputs_.put_batch(side, first, masks, count);
    }

    // Tick id of the next pair that advance() will apply.
    std::uint64_t next_tick() const noexcept { return inputs_.next_tick(); }

    const study_combat::Duel& state() const noexcept { return duel_; }

    Advance advance() noexcept {
        // A consumed tick space can never advance further.
        if (inputs_.exhausted()) {
            return Advance::exhausted;
        }

        // A finished board ends the match; do not consume the pending pair.
        if (duel_.left().finished() || duel_.right().finished()) {
            return Advance::finished;
        }

        // Both input records for this tick must exist; peek leaves them in place.
        std::uint8_t host_mask = 0;
        std::uint8_t peer_mask = 0;
        if (!inputs_.peek(host_mask, peer_mask)) {
            return Advance::waiting;
        }

        // Canonical mapping, independent of which process is running locally.
        const std::optional<study_input::Intent> left = study_input::decode(host_mask);
        const std::optional<study_input::Intent> right = study_input::decode(peer_mask);
        if (!left.has_value() || !right.has_value()) {
            return Advance::invalid;
        }

        // Apply to a copy first: a rejected step leaves duel_ and inputs_ intact.
        study_combat::Duel candidate = duel_;
        if (!candidate.tick(*left, *right).has_value()) {
            return Advance::invalid;
        }

        // Commit the duel, then retire the pair. peek and consume share this
        // single owner, so no interleaving can occur between the two calls.
        duel_ = candidate;
        // peek guaranteed both slot-0 inputs, so consume cannot fail here.
        (void)inputs_.consume();
        // An accepted pair advances next_tick() exactly once, even when the
        // step itself makes a board finished.
        return Advance::advanced;
    }

private:
    study_combat::Duel duel_;
    TickInputs inputs_;
};

} // namespace study_net
