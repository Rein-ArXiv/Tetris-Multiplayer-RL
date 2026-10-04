#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/lockstep.h"
#include "net/tick_inputs.h"
#include "simulation/duel.h"
#include "simulation/input_mask.h"

// Local-capture-timeline delay wrapper around Lockstep.
//
// The local player is captured once per fixed input-generation pulse, fully
// independently of how far the simulation has advanced. Each captured input is
// an *input record* (a tick's canonical mask); it is not a TCP frame and this
// header knows nothing about transport. Captures are staged at their own
// capture tick id with no reindexing: a local capture taken while
// next_capture_tick() == t is submitted to the Lockstep at wire tick t.
//
// The delay lives only in advance(), never in where records are stored. A tick
// is released to the simulation only once the local capture timeline has run
// delay() captures past it: local_limit() is a *local clock release ceiling*.
// It carries no evidence about the remote peer, whose exact presence is still
// checked by Lockstep::advance().
//
// Invariants:
//   * A single owner calls every operation, serialized. There are no locks, no
//     wall-clock reads, no sleeps and no I/O.
//   * Only a Put::stored capture advances next_capture_; too_far/invalid leave
//     the cursor untouched, so no input is dropped, skipped or silently shifted.
//   * Missing remote input always waits; an all-zero mask is present-neutral.
//   * The Lockstep 32-slot window bounds capture. delay() <= 30 permits
//     startup within the window; long remote stalls can still fill it.
//   * There is no automatic EOF drain: when captures stop, the last delay()
//     input records deliberately remain unconsumed. Callers define match
//     finalization separately.
namespace study_net {

class DelayedLockstep {
public:
    // delay is the local capture lead in ticks and must fit the Lockstep window.
    static std::optional<DelayedLockstep> create(const study_combat::Duel& duel, Side local,
                                                 unsigned delay, std::uint32_t first = 0) noexcept {
        if (local != Side::host && local != Side::peer) {
            return std::nullopt;
        }
        if (delay > 30u) {
            return std::nullopt;
        }
        return DelayedLockstep(duel, local, delay, first);
    }

    // Capture one local input record at the current capture tick. The mask is
    // validated before narrowing and before the tick-space check. The cursor
    // advances only on Put::stored, so a rejected capture is never reordered.
    Put capture(unsigned mask) noexcept {
        if (!study_input::valid(mask)) {
            return Put::invalid;
        }
        if (next_capture_ > static_cast<std::uint64_t>(UINT32_MAX)) {
            return Put::exhausted;
        }
        const std::uint32_t tick = static_cast<std::uint32_t>(next_capture_);
        const Put result = game_.submit(local_, tick, mask);
        if (result == Put::stored) {
            ++next_capture_;
        }
        return result;
    }

    // Stage one remote input record. Delegates to the opposite side only.
    Put receive(std::uint32_t tick, unsigned mask) noexcept {
        return game_.submit(remote(), tick, mask);
    }

    // Stage a contiguous run of remote input records. Lockstep's all-or-nothing
    // batch atomicity is preserved unchanged.
    Put receive_batch(std::uint32_t first, const std::uint8_t* masks,
                      std::size_t count) noexcept {
        return game_.submit_batch(remote(), first, masks, count);
    }

    // Capture tick of the next local input record to be captured.
    std::uint64_t next_capture_tick() const noexcept { return next_capture_; }

    // Tick id of the next pair that advance() will apply.
    std::uint64_t next_tick() const noexcept { return game_.next_tick(); }

    std::uint8_t delay() const noexcept { return delay_; }

    const study_combat::Duel& state() const noexcept { return game_.state(); }

    // Highest wire tick the local capture timeline permits the simulation to
    // apply. A pure local clock ceiling, never evidence of remote presence.
    // Widened to int64 so it can reach UINT32_MAX and go negative before the
    // first delay() captures exist.
    std::int64_t local_limit() const noexcept {
        return static_cast<std::int64_t>(next_capture_) - 1 -
               static_cast<std::int64_t>(delay_);
    }

    Advance advance() noexcept {
        const std::uint64_t tick = game_.next_tick();

        // Beyond the wire tick space, or with a finished board, Lockstep owns
        // the result (exhausted/finished). Report it even though tick is past
        // local_limit(): an exhausted space is not a waiting result.
        if (tick > static_cast<std::uint64_t>(UINT32_MAX) || game_.state().left().finished() ||
            game_.state().right().finished()) {
            return game_.advance();
        }

        // Local clock gate: reserve delay() future captures before release.
        // Remote arrivals are never subtracted from this frontier, so buffered
        // reserves stay spendable when future remote input stalls.
        if (static_cast<std::int64_t>(tick) > local_limit()) {
            return Advance::waiting;
        }

        // Within the local ceiling: Lockstep checks exact remote presence.
        return game_.advance();
    }

private:
    DelayedLockstep(const study_combat::Duel& duel, Side local, unsigned delay,
                    std::uint32_t first) noexcept
        : game_(duel, first), local_(local), delay_(static_cast<std::uint8_t>(delay)),
          next_capture_(first) {}

    Side remote() const noexcept {
        return local_ == Side::host ? Side::peer : Side::host;
    }

    Lockstep game_;
    Side local_;
    std::uint8_t delay_;
    std::uint64_t next_capture_;
};

} // namespace study_net
