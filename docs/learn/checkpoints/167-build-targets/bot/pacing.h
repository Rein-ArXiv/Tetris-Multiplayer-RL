#ifndef BOT_TICK_GATE_HPP
#define BOT_TICK_GATE_HPP

#include <algorithm>
#include <stdexcept>

namespace bot {

// Pacing controls when the bot may act.
//   interval : cooldown length; defer() blocks the next interval-1 ticks
//   think    : ticks the gate must age before an operation is considered
//   minimum  : ticks the gate must age before a drop is ready
struct Pacing {
    static constexpr int interval_min = 1;
    static constexpr int interval_max = 30;
    static constexpr int think_min = 0;
    static constexpr int think_max = 180;
    static constexpr int minimum_min = 1;
    static constexpr int minimum_max = 600;

    int interval = 6;
    int think = 18;
    int minimum = 60;

    // Clamp each field into its valid range.
    static Pacing clamped(int interval, int think, int minimum) noexcept {
        return Pacing{std::clamp(interval, interval_min, interval_max),
                      std::clamp(think, think_min, think_max),
                      std::clamp(minimum, minimum_min, minimum_max)};
    }

    bool valid() const noexcept {
        return interval >= interval_min && interval <= interval_max &&
               think >= think_min && think <= think_max &&
               minimum >= minimum_min && minimum <= minimum_max;
    }
};

// TickGate tracks gate age and cooldown.
//
// age_ is a saturated gate counter, not total elapsed simulation time: it
// grows by one per begin_tick() until it reaches max(think, minimum) and then
// stays there. Each update uses bounded integer arithmetic.
class TickGate {
public:
    TickGate() = default;

    explicit TickGate(Pacing pacing) : pacing_(pacing) {
        if (!pacing_.valid()) throw std::invalid_argument("invalid Pacing");
    }

    // Validate first; on success store and start a new piece. On failure
    // throw std::invalid_argument and leave existing state unchanged.
    void reset(Pacing pacing) {
        if (!pacing.valid()) throw std::invalid_argument("invalid Pacing");
        pacing_ = pacing;
        new_piece();
    }

    void new_piece() noexcept {
        age_ = 0;
        cooldown_ = 0;
    }

    // Call exactly once per real simulation tick. Returns true when the gate
    // is open for an operation this tick.
    bool begin_tick() noexcept {
        const int old_age = age_;
        const int cap = std::max(pacing_.think, pacing_.minimum);
        if (age_ < cap) ++age_; // saturate at cap
        if (old_age < pacing_.think) return false;
        if (cooldown_ > 0) {
            --cooldown_;
            return false;
        }
        return true;
    }

    // Ready once the saturated gate counter reaches minimum.
    bool drop_ready() const noexcept {
        return age_ >= pacing_.minimum;
    }

    // Block the next interval-1 ticks.
    void defer() noexcept {
        cooldown_ = pacing_.interval - 1;
    }

    const Pacing& config() const noexcept { return pacing_; }
    int age_for_gates() const noexcept { return age_; }
    int cooldown() const noexcept { return cooldown_; }

private:
    Pacing pacing_{};
    int age_ = 0;
    int cooldown_ = 0;
};

} // namespace bot

#endif
