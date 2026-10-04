#pragma once

#include <cstdint>
#include <stdexcept>

namespace study_net {

// Pacing policy for a discrete tick clock. Every field is caller-owned;
// this header provides no rate defaults.
struct PacePolicy {
    uint32_t ticks_per_second;  // tick rate, 1..1'000'000'000
    uint64_t lead_ticks;        // allowance granted at elapsed time zero
    uint64_t max_ticks;         // hard ceiling on the allowance
};

// Pure integer allowance helper: no clocks, no sockets, no I/O.
class TickAllowance {
public:
    // One second expressed in nanoseconds.
    static constexpr uint64_t nanos_per_second = 1000000000ULL;

    // Validates the policy and stores it. Throws std::invalid_argument on bad input.
    explicit TickAllowance(PacePolicy policy) : policy_(validate(policy)) {}

    const PacePolicy& policy() const noexcept { return policy_; }

    // Ticks permitted after elapsed_ns nanoseconds, exactly
    //   min(max_ticks, lead_ticks + floor(elapsed_ns * ticks_per_second / 1e9))
    // for every uint64_t input, using integer arithmetic only.
    //
    // Ticks are zero-based: tick 0 is the first tick. A consumer that admits the
    // exclusive end point `first_tick + count` by testing `<= allowance` therefore
    // accepts ticks 0..allowance-1.
    uint64_t allowed(uint64_t elapsed_ns) const noexcept {
        const uint64_t rate = policy_.ticks_per_second;
        const uint64_t max = policy_.max_ticks;

        // Split the elapsed time so no product can overflow.
        const uint64_t whole_seconds = elapsed_ns / nanos_per_second;
        const uint64_t remainder_ns = elapsed_ns % nanos_per_second;

        // Cap the whole-second contribution before multiplying it by the rate:
        // once whole_seconds * rate reaches max, the result is max_ticks no matter
        // what lead_ticks or the remainder contribute. The bound uses the exact
        // ceiling (max + rate - 1) / rate, so a second that could still land below
        // max_ticks is never discarded.
        const uint64_t seconds_cap = (max + rate - 1) / rate;
        if (whole_seconds >= seconds_cap) {
            return max;
        }

        // Below the cap, whole_seconds * rate < max <= UINT32_MAX + 1, so the
        // multiplication stays inside uint64_t.
        uint64_t ticks = policy_.lead_ticks + whole_seconds * rate;
        if (ticks >= max) {
            return max;  // cap before adding the sub-second part
        }

        // remainder_ns < 1e9 and rate <= 1e9, so remainder_ns * rate < 1e18 and
        // fits in uint64_t.
        const uint64_t fractional = (remainder_ns * rate) / nanos_per_second;

        // Cap once more before adding, so the sum cannot wrap.
        if (fractional >= max - ticks) {
            return max;
        }
        return ticks + fractional;
    }

private:
    static PacePolicy validate(const PacePolicy& policy) {
        if (policy.ticks_per_second < 1 || policy.ticks_per_second > 1000000000ULL) {
            throw std::invalid_argument("ticks_per_second must be in [1, 1000000000]");
        }
        if (policy.max_ticks < 1 || policy.max_ticks > static_cast<uint64_t>(UINT32_MAX) + 1) {
            throw std::invalid_argument("max_ticks must be in [1, UINT32_MAX+1]");
        }
        if (policy.lead_ticks > policy.max_ticks) {
            throw std::invalid_argument("lead_ticks must not exceed max_ticks");
        }
        return policy;
    }

    PacePolicy policy_;
};

}  // namespace study_net
