#pragma once

// Bounded tutorial frame runner: one transactional step of simulation.
// No callbacks, no IO, no platform/GL, no wall-clock reads. The clock only
// supplies a duration budget; this header never asks what time it is.

#include <array>
#include <cstdint>
#include <optional>

#include "simulation/pending_controls.h"
#include "simulation/round.h"
#include "timing/fixed_clock.h"

namespace study_loop {
using study_timing::FixedClock;

struct FrameInput {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool drop = false;
};

struct LockReport {
    int cleared = 0;
    int hard_drop_distance = -1;
    int spin_lines = -1;
    int inserted = 0;
    int pending = 0;
    std::uint64_t points = 0;
    std::uint64_t awarded = 0;
    std::uint64_t lines = 0;
    std::uint64_t attack_total = 0;
    std::uint64_t streak = 0;
    unsigned level = 0;
    int interval = 0;
    study_round::EndReason end_reason{};
};

struct TickObservation {
    study_round::Step step = study_round::Step::waiting;
    int kick = -1;
    std::optional<LockReport> lock;
};

// The clock is fixed-rate and its phase is always < units, so an advance of at
// most max_frame_ns schedules at most max_frame_ns * rate / units ticks.
constexpr unsigned max_ticks = 6;

static_assert(FixedClock::units > 0, "positive phase denominator");
static_assert((FixedClock::max_frame_ns * FixedClock::rate) % FixedClock::units == 0,
              "this bound assumes a whole number of ticks per maximum frame");
static_assert((FixedClock::max_frame_ns * FixedClock::rate) / FixedClock::units == max_ticks,
              "report capacity must match the clock's maximum batch");

struct FrameReport {
    unsigned ticks = 0;
    bool clamped = false;
    bool board_changed = false;
    bool piece_changed = false;
    int cleared = 0;
    std::array<TickObservation, max_ticks> observations{};
};

class FrameRunner {
public:
    explicit FrameRunner(study_round::Round round) noexcept : round_(round) {}

    const study_round::Round& round() const noexcept { return round_; }

    std::uint64_t phase() const noexcept { return clock_.phase(); }

    std::optional<FrameReport> advance(double seconds, FrameInput raw) noexcept {
        // Work on a private copy; publish only after the whole frame succeeds.
        FrameRunner candidate = *this;

        const auto batch = candidate.clock_.advance_seconds(seconds);
        if (!batch.has_value()) {
            // nan / inf / negative: nothing about the original may change.
            return std::nullopt;
        }

        if (batch->ticks > max_ticks) return std::nullopt; // Never silently truncate work.

        // Capture exactly once, even for a zero-tick frame, so a latched edge
        // is not lost and the latest soft-drop state survives.
        candidate.pending_.capture(raw.left, raw.right, raw.up, raw.down, raw.drop);

        FrameReport report{};
        report.clamped = batch->clamped;

        for (unsigned i = 0; i < batch->ticks; ++i) {
            const auto controls = candidate.pending_.consume();

            const study_round::Step step = candidate.round_.tick(
                controls.horizontal, controls.clockwise, controls.soft_drop, controls.hard_drop);

            if (step == study_round::Step::invalid) {
                return std::nullopt;
            }

            TickObservation observation{};
            observation.step = step;

            // A stopped round exposes no fresh rotation event; old reports may
            // still be readable inside the Round and must not be re-published.
            if (step != study_round::Step::stopped) {
                observation.kick = candidate.round_.last_rotation_candidate();
            }

            if (step == study_round::Step::locked || step == study_round::Step::game_over) {
                // Snapshot every field right now; never borrow from the Round.
                LockReport lock{};
                lock.cleared = candidate.round_.last_cleared();
                lock.hard_drop_distance = candidate.round_.last_hard_drop_distance();
                lock.spin_lines = candidate.round_.last_t_spin_lines();
                lock.inserted = candidate.round_.last_garbage();
                lock.pending = candidate.round_.pending_garbage();
                lock.points = candidate.round_.score();
                lock.awarded = candidate.round_.last_awarded();
                lock.lines = candidate.round_.total_lines();
                lock.attack_total = candidate.round_.attack_sent();
                lock.streak = candidate.round_.clear_streak();
                lock.level = candidate.round_.level();
                lock.interval = candidate.round_.gravity().interval;
                lock.end_reason = candidate.round_.end_reason();

                observation.lock = lock;

                report.cleared += lock.cleared;
                report.board_changed = true;
                report.piece_changed = true;
            }

            if (step == study_round::Step::changed) {
                report.piece_changed = true;
            }

            report.observations[i] = observation;
            report.ticks = i + 1;  // counted incl. a stopped tick.
        }

        // Commit and hand back a self-contained snapshot, not a view.
        *this = candidate;
        return report;
    }

private:
    study_round::Round round_;
    FixedClock clock_{};
    study_input::PendingControls pending_{};
};

}  // namespace study_loop
