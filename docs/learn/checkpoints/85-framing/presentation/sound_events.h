#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "loop/frame_runner.h"

namespace study_sound {

enum class Kind { rotate, drop, clear, garbage };

// Move-only projection of one frame's simulation report into ordered sound
// cues. At most four cues are produced per tick, in the fixed order rotate,
// drop, clear, garbage, so the fixed array is sized to the runner's proven
// batch bound. Projecting the same FrameReport twice yields the cues twice:
// this is single-owner batch consumption, not global exactly-once delivery.
class Batch {
public:
    Batch() noexcept = default;

    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;

    Batch(Batch&& other) noexcept { move_from(other); }

    Batch& operator=(Batch&& other) noexcept {
        if (this != &other) {
            clear();
            move_from(other);
        }
        return *this;
    }

    // Validate the reported tick count before reading observations so a
    // malformed report can never walk past the fixed array.
    static std::optional<Batch> from(
        const study_loop::FrameReport& report) noexcept {
        if (report.ticks > report.observations.size()) return std::nullopt;

        Batch batch;
        for (unsigned i = 0; i < report.ticks; ++i) {
            const study_loop::TickObservation& observation =
                report.observations[i];
            if (observation.step == study_round::Step::stopped ||
                observation.step == study_round::Step::invalid) {
                continue;
            }
            if (observation.kick >= 0) batch.append(Kind::rotate);
            if (observation.lock.has_value()) {
                if (observation.lock->hard_drop_distance >= 0)
                    batch.append(Kind::drop);
                if (observation.lock->cleared > 0) batch.append(Kind::clear);
                if (observation.lock->inserted > 0)
                    batch.append(Kind::garbage);
            }
        }
        return std::optional<Batch>(std::move(batch));
    }

    // Yield the next cue, advancing the cursor before returning it. An empty
    // or exhausted batch returns nullopt.
    std::optional<Kind> take() noexcept {
        if (next_ >= size_) return std::nullopt;
        const Kind kind = kinds_[next_];
        ++next_;
        return kind;
    }

    bool empty() const noexcept { return next_ >= size_; }
    std::size_t remaining() const noexcept { return size_ - next_; }

private:
    // Four cues per tick times the runner's maximum tick batch.
    static constexpr std::size_t kCapacity = study_loop::max_ticks * 4;

    // from() proved ticks <= observations.size() == max_ticks, and each
    // observation appends at most rotate/drop/clear/garbage, so the fixed
    // array can never overflow.
    void append(Kind kind) noexcept {
        kinds_[size_] = kind;
        ++size_;
    }

    void clear() noexcept {
        size_ = 0;
        next_ = 0;
    }

    // Move preserves the full cursor (size_ and next_) so the destination
    // keeps the remaining cues, then empties the source.
    void move_from(Batch& other) noexcept {
        kinds_ = other.kinds_;
        size_ = other.size_;
        next_ = other.next_;
        other.clear();
    }

    std::array<Kind, kCapacity> kinds_{};
    std::size_t size_ = 0;
    std::size_t next_ = 0;
};

}  // namespace study_sound
