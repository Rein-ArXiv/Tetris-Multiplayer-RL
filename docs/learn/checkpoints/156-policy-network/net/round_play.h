#pragma once
#include "net/delayed_lockstep.h"
#include "net/round_gate.h"
#include "net/round_protocol.h"

namespace study_net {
// input holds the detailed TickInputs result only when scope is current.
struct RoundInputResult {
    RoundScope scope = RoundScope::inactive;
    Put input = Put::invalid;
};

// One main-thread owner composes lifecycle, input history, and simulation.
// The caller coordinates round id/seed/roles with its peer before prepare().
class RoundPlay {
public:
    bool prepare(std::uint64_t id, const study_combat::Duel& duel, Side local,
                 unsigned delay, std::uint32_t first = 0) noexcept {
        RoundGate candidate = gate_;
        if (!candidate.prepare(id)) return false;
        auto game = DelayedLockstep::create(duel, local, delay, first);
        if (!game || duel.left().finished() || duel.right().finished()) return false;
        game_ = *game; // Replace the old tick history only after both checks.
        gate_ = candidate;
        return true;
    }
    bool start() noexcept { return gate_.start(); }
    bool finish() noexcept { return gate_.finish(); }
    const RoundGate& gate() const noexcept { return gate_; }
    // Null before the first successful prepare; read-only access never admits input.
    const DelayedLockstep* game() const noexcept { return game_ ? &*game_ : nullptr; }

    RoundInputResult capture(unsigned mask, Frame& outgoing) noexcept {
        if (!gate_.can_capture()) return {};
        auto candidate = *game_;
        const auto tick = candidate.next_capture_tick();
        const Put result = candidate.capture(mask);
        if (result != Put::stored) return {RoundScope::current, result};
        RoundBatch batch;
        batch.round = gate_.round();
        batch.inputs.first_tick = static_cast<std::uint32_t>(tick);
        batch.inputs.count = 1;
        batch.inputs.masks[0] = static_cast<std::uint8_t>(mask);
        Frame encoded;
        if (!encode_round_input(batch, encoded)) return {RoundScope::current, Put::invalid};
        *game_ = candidate;
        outgoing = encoded;
        return {RoundScope::current, Put::stored};
    }

    RoundInputResult receive(const Frame& frame) noexcept {
        RoundBatch batch;
        if (!decode_round_input(frame, batch)) return {RoundScope::invalid_frame, Put::invalid};
        const auto scope = gate_.classify(batch.round);
        if (scope != RoundScope::current) return {scope, Put::invalid};
        return {scope, game_->receive_batch(batch.inputs.first_tick,
                                           batch.inputs.masks.data(), batch.inputs.count)};
    }

    Advance advance() noexcept {
        if (gate_.phase() == RoundPhase::ended) return Advance::finished;
        if (gate_.phase() != RoundPhase::playing) return Advance::waiting;
        const auto result = game_->advance();
        if (result == Advance::finished || result == Advance::exhausted ||
            game_->state().left().finished() || game_->state().right().finished())
            (void)gate_.finish();
        return result;
    }
private:
    RoundGate gate_;
    std::optional<DelayedLockstep> game_;
};
} // namespace study_net
