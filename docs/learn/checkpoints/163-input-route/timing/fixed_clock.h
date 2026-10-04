#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

namespace study_timing {
struct Batch { unsigned ticks; bool clamped; };

// Host adapter: integer nanoseconds become rational 60 Hz phase credits.
// This is outside the simulation. A tick itself never reads wall time.
class FixedClock {
public:
    static constexpr std::uint64_t rate = 60;
    static constexpr std::uint64_t units = 1'000'000'000;
    static constexpr std::uint64_t max_frame_ns = 100'000'000;

    Batch advance_ns(std::uint64_t elapsed_ns) noexcept {
        const bool clamped = elapsed_ns > max_frame_ns;
        const auto accepted = clamped ? max_frame_ns : elapsed_ns;
        // Clamp before multiplying: phase < 1e9, total < 7e9.
        const auto total = phase_ + accepted * rate;
        phase_ = total % units;
        return {static_cast<unsigned>(total / units), clamped};
    }

    std::optional<Batch> advance_seconds(double seconds) noexcept {
        if (!std::isfinite(seconds) || seconds < 0.0) return std::nullopt;
        const bool clamped = seconds > 0.1;
        // Bound before floating-to-integer conversion, even for enormous dt.
        const double accepted = clamped ? 0.1 : seconds;
        const auto ns = static_cast<std::uint64_t>(accepted * 1'000'000'000.0);
        auto batch = advance_ns(ns);
        batch.clamped = clamped;
        return batch;
    }

    std::uint64_t phase() const noexcept { return phase_; }
private:
    std::uint64_t phase_ = 0;
};
} // namespace study_timing
