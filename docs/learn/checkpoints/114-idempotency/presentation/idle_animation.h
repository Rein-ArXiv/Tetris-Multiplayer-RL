#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

#include "core/rng.h"

// Study-room idle animation: an isolated decorative clock plus deterministic offsets.
// No rule/simulation state is read and no GL calls are made; callers pull a Frame
// each render frame and blend it into their own render path.
namespace study_idle {
static_assert(std::numeric_limits<double>::radix==2 &&
              std::numeric_limits<double>::digits>=53,
              "idle offsets require at least 53 binary digits");

// Fixed-period timeline advanced only when the caller marks it active.
class Clock {
public:
    static constexpr double period_seconds = 4.0;
    static constexpr double max_step_seconds = 0.1;

    // Non-finite or negative input is rejected and leaves the clock untouched,
    // even while inactive, so bad frame times cannot poison the timeline.
    // A valid inactive step is a no-op success; a valid active step is clamped
    // to max_step_seconds; excess elapsed time is discarded, not queued.
    bool advance(double elapsed, bool active) noexcept {
        if (!std::isfinite(elapsed) || elapsed < 0.0) return false;
        if (!active) return true;
        const double step = elapsed < max_step_seconds ? elapsed : max_step_seconds;
        seconds_ = std::fmod(seconds_ + step, period_seconds);
        return true;
    }

    double seconds() const noexcept { return seconds_; }
    double phase() const noexcept { return seconds_ / period_seconds; } // [0,1)

private:
    double seconds_ = 0.0;
};

// Snapshot of the animated decoration strengths for one rendered frame.
struct Frame {
    double border_alpha = 1.0;
    double portrait_alpha = 1.0;
    double decoration_degrees = 30.0;
};

// Per-session animation parameters. The offsets come from a local RNG instance
// seeded explicitly. Offsets repeat for the same seed; sin/GL pixel results need
// not be bitwise identical across platforms.
class Animation final {
public:
    // Seed 0 falls back to the RNG's own engine default.
    explicit Animation(std::uint64_t seed = 0x1d1e1717ULL)
        : border_offset_(0.0), portrait_offset_(0.0) {
        // Local, short-lived RNG: only the two offsets must persist.
        XorShift64Star rng(seed);
        const double inv_2_53 = 1.0 / 9007199254740992.0; // 2^-53
        border_offset_ = static_cast<double>(rng.next() >> 11) * inv_2_53;
        portrait_offset_ = static_cast<double>(rng.next() >> 11) * inv_2_53;
    }

    bool advance(double elapsed, bool active) noexcept {
        return clock_.advance(elapsed, active);
    }

    // Pure and repeatable: no RNG draws and no state mutation. A disabled
    // sample is static (Frame defaults) and independent of the stored phase;
    // a paused clock simply resumes from its last phase when reactivated.
    Frame sample(bool enabled) const noexcept {
        Frame frame;
        if (!enabled) return frame;

        const double phase = clock_.phase();
        const double border_wave = wave(phase + border_offset_);
        const double portrait_wave = wave(phase + portrait_offset_);
        frame.border_alpha = 0.65 + 0.35 * border_wave;
        frame.portrait_alpha = 0.85 + 0.15 * portrait_wave;
        frame.decoration_degrees = 360.0 * phase; // [0,360)
        return frame;
    }

    double seconds() const noexcept { return clock_.seconds(); }
    double border_offset() const noexcept { return border_offset_; }
    double portrait_offset() const noexcept { return portrait_offset_; }

private:
    static constexpr double pi = 3.14159265358979323846;

    // Wraps the nonnegative phase + offset into [0,1).
    static double wrap01(double p) noexcept {
        double w = std::fmod(p, 1.0);
        return w;
    }

    // 0.5 + 0.5*sin(2*pi*p), clamped to defend against tiny float excursions
    // outside [0,1] before it feeds alpha math.
    static double wave(double p) noexcept {
        const double w = 0.5 + 0.5 * std::sin(2.0 * pi * wrap01(p));
        return w < 0.0 ? 0.0 : (w > 1.0 ? 1.0 : w);
    }

    Clock clock_;
    double border_offset_;
    double portrait_offset_;
};

} // namespace study_idle
