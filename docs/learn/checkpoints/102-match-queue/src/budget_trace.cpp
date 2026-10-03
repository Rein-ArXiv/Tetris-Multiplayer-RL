// Compare disposal and carry policies with explicit scenario boundaries.
#include "timing/catch_up_clock.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

const char* policy_name(study_budget::Policy p) noexcept {
    switch (p) {
        case study_budget::Policy::clamp_elapsed:   return "clamp_elapsed";
        case study_budget::Policy::keep_backlog:    return "keep_backlog";
        case study_budget::Policy::discard_backlog: return "discard_backlog";
    }
    return "unknown";
}

void emit(study_budget::Policy policy, const char* scenario, unsigned frame, std::uint64_t input_ns,
          study_budget::CatchUpClock& clock) {
    const auto report = clock.advance_ns(input_ns);
    if (!report) {
        std::fprintf(stderr, "budget_trace: policy=%s frame=%u input_ns=%llu rejected\n",
                     policy_name(policy), frame,
                     static_cast<unsigned long long>(input_ns));
        std::exit(1);
    }
    std::printf("%s,%s,%u,%llu,%u,%llu,%llu,%llu,%llu\n", policy_name(policy), scenario, frame,
                static_cast<unsigned long long>(input_ns), report->ticks,
                static_cast<unsigned long long>(report->pending_ticks),
                static_cast<unsigned long long>(report->phase),
                static_cast<unsigned long long>(report->clamped_ns),
                static_cast<unsigned long long>(report->dropped_ticks));
}

} // namespace

int main() {
    constexpr std::uint64_t one_second_ns = 1'000'000'000ULL;
    constexpr std::uint64_t fraction_ns = 16'666'667ULL; // 1 tick + 20 units

    const study_budget::Policy policies[] = {study_budget::Policy::clamp_elapsed,
                                             study_budget::Policy::keep_backlog,
                                             study_budget::Policy::discard_backlog};

    std::puts("policy,scenario,frame,input_ns,ticks,pending_ticks,phase,clamped_ns,dropped_ticks");

    for (auto policy : policies) {
        study_budget::CatchUpClock clock(policy);
        emit(policy, "burst", 1, one_second_ns, clock);
        emit(policy, "burst", 2, 0, clock);
        emit(policy, "burst", 3, 0, clock);

        // 새 clock: sub-tick fraction 이 보존되는지 보여준다(모든 정책 동일).
        study_budget::CatchUpClock fresh(policy);
        emit(policy, "boundary", 1, fraction_ns, fresh);
    }
    return 0;
}
