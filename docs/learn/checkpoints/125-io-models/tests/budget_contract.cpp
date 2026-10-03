// 46차시 계약 테스트. assert 대신 CHECK(실패 시 행 번호 + exit 1)를 써서
// Release 빌드에서도 항상 활성화한다.
//
// 검사 항목:
//   * 규격 상수를 구현과 독립적인 literal(60, 1e9, 6)로 다시 적어 대조.
//   * 매 prefix 마다 시간 보존식 성립.
//   * 정책 불변식(ticks<=6, phase<U, clamp/drop pending==0, keep drop/clamp==0).
//   * 1s 쏠림 후 0ns drain, 6틱+1ns fraction, UINT64max 거부 시 상태 보존,
//     clamp 의 큰 입력 성공.
//   * 기존 study_timing::FixedClock 과 clamp 정책의 동등성 + 독립 보존식.
//
// CPU 전용. SDL/GL 미포함. 기존 소스는 수정하지 않는다.

#include "timing/catch_up_clock.h"
#include "timing/fixed_clock.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,     \
                         __LINE__, #cond);                                    \
            std::exit(1);                                                     \
        }                                                                     \
    } while (0)

namespace {

// 구현과 독립적인 literal 규격값.
constexpr std::uint64_t kRate = 60;                // [tick/s]
constexpr std::uint64_t kUnits = 1'000'000'000;    // [unit/tick]
constexpr std::uint64_t kMaxFrameNs = 100'000'000; // [ns]
constexpr unsigned kBudget = 6;                    // [tick]

using study_budget::Policy;

// 테스트 입력 생성 전용 64-bit LCG.
std::uint64_t next_rng(std::uint64_t& state) noexcept {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return state;
}

void check_constants() {
    CHECK(study_budget::CatchUpClock::rate == kRate);
    CHECK(study_budget::CatchUpClock::units == kUnits);
    CHECK(study_budget::CatchUpClock::max_frame_ns == kMaxFrameNs);
    CHECK(study_budget::CatchUpClock::budget == kBudget);
}

// 한 정책에 대해 입력 시퀀스를 돌리며 prefix 시간 보존식과 정책 불변식을 검사한다.
void run_sequence(Policy policy, const std::uint64_t* inputs, std::size_t count) {
    study_budget::CatchUpClock clock(policy);

    std::uint64_t input_total_ns = 0;
    std::uint64_t accepted_total_ns = 0; // 정책이 실제로 수락한 ns (독립 누적)
    std::uint64_t issued_total = 0;      // [tick]
    std::uint64_t clamped_total_ns = 0;  // [ns]
    std::uint64_t dropped_total = 0;     // [tick]

    for (std::size_t i = 0; i < count; ++i) {
        const std::uint64_t ns = inputs[i];
        const auto report = clock.advance_ns(ns);
        CHECK(report.has_value());

        input_total_ns += ns;
        issued_total += report->ticks;
        clamped_total_ns += report->clamped_ns;
        dropped_total += report->dropped_ticks;
        accepted_total_ns += (policy == Policy::clamp_elapsed && ns > kMaxFrameNs)
                                 ? kMaxFrameNs
                                 : ns;

        // 정책 불변식.
        CHECK(report->ticks <= kBudget);
        CHECK(clock.phase() < kUnits);
        CHECK(clock.phase() == report->phase);
        CHECK(clock.backlog_ticks() == report->pending_ticks);

        if (policy == Policy::clamp_elapsed) {
            CHECK(report->pending_ticks == 0);
            CHECK(clock.backlog_ticks() == 0);
            CHECK(report->dropped_ticks == 0);
            CHECK((ns > kMaxFrameNs) == (report->clamped_ns != 0));
        } else if (policy == Policy::keep_backlog) {
            CHECK(report->clamped_ns == 0);
            CHECK(report->dropped_ticks == 0);
        } else { // discard_backlog
            CHECK(report->pending_ticks == 0);
            CHECK(clock.backlog_ticks() == 0);
            CHECK(report->clamped_ns == 0);
        }

        // prefix 시간 보존식:
        //   input_total_ns * 60 == issued_total*U + backlog*U + phase
        //                          + clamped_total_ns * 60 + dropped_total * U
        CHECK(input_total_ns * kRate ==
              issued_total * kUnits + clock.backlog_ticks() * kUnits +
                  clock.phase() + clamped_total_ns * kRate +
                  dropped_total * kUnits);

        // 수락 ns 기반 독립 보존식(운영 구현 수식을 복제하지 않는다).
        CHECK(accepted_total_ns * kRate ==
              issued_total * kUnits + clock.backlog_ticks() * kUnits +
                  clock.phase() + dropped_total * kUnits);
    }
}

void check_lcg_sequences() {
    constexpr std::size_t kSamples = 10000;
    std::vector<std::uint64_t> inputs;
    inputs.reserve(kSamples);
    std::uint64_t rng = 0x1234'5678'9abc'def0ULL;
    for (std::size_t i = 0; i < kSamples; ++i) {
        inputs.push_back(next_rng(rng) % 200'000'001ULL); // 0..200ms, 합이 작아 overflow 없음
    }
    run_sequence(Policy::clamp_elapsed, inputs.data(), inputs.size());
    run_sequence(Policy::keep_backlog, inputs.data(), inputs.size());
    run_sequence(Policy::discard_backlog, inputs.data(), inputs.size());
}

void check_keep_drain() {
    study_budget::CatchUpClock clock(Policy::keep_backlog);
    const auto burst = clock.advance_ns(1'000'000'000ULL); // 1s == 60 틱
    CHECK(burst.has_value());
    CHECK(burst->ticks == 6);
    CHECK(burst->pending_ticks == 54);
    CHECK(burst->phase == 0);

    std::uint64_t issued_total = burst->ticks;
    for (int i = 0; i < 10; ++i) { // 0ns 호출로 backlog 만 소진
        const auto drained = clock.advance_ns(0);
        CHECK(drained.has_value());
        CHECK(drained->ticks <= kBudget);
        CHECK(drained->phase == 0);
        issued_total += drained->ticks;
    }
    CHECK(clock.backlog_ticks() == 0);
    CHECK(clock.phase() == 0);
    CHECK(issued_total == 60); // 1s 분량 전체가 결국 발행됨
}

void check_fraction_phase() {
    // 100ms is six ticks. The extra 1ns is clamped only by clamp_elapsed.
    for (Policy policy : {Policy::clamp_elapsed, Policy::keep_backlog,
                          Policy::discard_backlog}) {
        study_budget::CatchUpClock clock(policy);
        const auto report = clock.advance_ns(100'000'001ULL);
        CHECK(report.has_value());
        CHECK(report->ticks == 6);
        CHECK(report->pending_ticks == 0);
        CHECK(report->dropped_ticks == 0);
        CHECK(report->clamped_ns == (policy == Policy::clamp_elapsed ? 1 : 0));
        CHECK(clock.phase() == (policy == Policy::clamp_elapsed ? 0 : 60));
    }
    // With existing fractional time, both disposal policies still preserve it.
    for (Policy policy : {Policy::clamp_elapsed, Policy::keep_backlog,
                          Policy::discard_backlog}) {
        study_budget::CatchUpClock clock(policy);
        CHECK(clock.advance_ns(1)->phase == 60);
        const auto burst = clock.advance_ns(1'000'000'000ULL);
        CHECK(burst.has_value());
        CHECK(burst->ticks == 6 && burst->phase == 60);
        CHECK(burst->pending_ticks == (policy == Policy::keep_backlog ? 54 : 0));
        CHECK(burst->dropped_ticks == (policy == Policy::discard_backlog ? 54 : 0));
    }
}

void check_rejection_preserves_state() {
    for (Policy policy : {Policy::keep_backlog, Policy::discard_backlog}) {
        study_budget::CatchUpClock clock(policy);
        const auto primed = clock.advance_ns(150'000'001ULL); // Nonzero phase; keep policy also has backlog
        CHECK(primed.has_value());

        const std::uint64_t phase_before = clock.phase();
        const std::uint64_t backlog_before = clock.backlog_ticks();

        const auto rejected = clock.advance_ns(UINT64_MAX);
        CHECK(!rejected.has_value()); // 거대 입력은 거부
        CHECK(clock.phase() == phase_before);
        CHECK(clock.backlog_ticks() == backlog_before);

        const auto after = clock.advance_ns(0); // 거부 뒤에도 정상
        CHECK(after.has_value());
    }

    // Addition capacity matters even when the multiplication alone fits.
    study_budget::CatchUpClock nearly_full(Policy::keep_backlog);
    CHECK(nearly_full.advance_ns(UINT64_MAX / 60).has_value());
    const auto saved_phase = nearly_full.phase();
    const auto saved_backlog = nearly_full.backlog_ticks();
    CHECK(!nearly_full.advance_ns(101'000'000).has_value());
    CHECK(nearly_full.phase() == saved_phase);
    CHECK(nearly_full.backlog_ticks() == saved_backlog);

    study_budget::CatchUpClock invalid(static_cast<Policy>(99));
    CHECK(!invalid.advance_ns(1).has_value());
    CHECK(invalid.phase() == 0 && invalid.backlog_ticks() == 0);

    // clamp 는 UINT64_MAX 입력도 성공해야 한다(100ms 로 잘림).
    study_budget::CatchUpClock clamp(Policy::clamp_elapsed);
    const auto big = clamp.advance_ns(UINT64_MAX);
    CHECK(big.has_value());
    CHECK(big->ticks == 6);
    CHECK(big->clamped_ns == UINT64_MAX - kMaxFrameNs);
    CHECK(clamp.phase() == 0);
    CHECK(clamp.backlog_ticks() == 0);
}

void check_fixed_clock_equivalence() {
    constexpr std::size_t kSamples = 2000;
    std::uint64_t rng = 0x0fed'cba9'8765'4321ULL;

    study_budget::CatchUpClock clock(Policy::clamp_elapsed);
    study_timing::FixedClock fixed;

    std::uint64_t input_total_ns = 0;
    std::uint64_t accepted_total_ns = 0;
    std::uint64_t issued_total = 0;
    std::uint64_t clamped_total_ns = 0;

    for (std::size_t i = 0; i < kSamples; ++i) {
        const std::uint64_t ns = next_rng(rng) % 200'000'001ULL;
        const auto report = clock.advance_ns(ns);
        const auto batch = fixed.advance_ns(ns);
        CHECK(report.has_value());

        // 동등성: 같은 ns 표본에 대해 발행 틱/잔여 phase/클램프 여부가 일치.
        CHECK(report->ticks == batch.ticks);
        CHECK(clock.phase() == fixed.phase());
        CHECK((report->clamped_ns != 0) == batch.clamped);
        CHECK(report->pending_ticks == 0);
        CHECK(report->dropped_ticks == 0);

        input_total_ns += ns;
        accepted_total_ns += batch.clamped ? kMaxFrameNs : ns;
        issued_total += report->ticks;
        clamped_total_ns += report->clamped_ns;

        // 독립 보존식 (수락 ns 및 원시 ns 기준).
        CHECK(accepted_total_ns * kRate == issued_total * kUnits + clock.phase());
        CHECK(input_total_ns * kRate ==
              issued_total * kUnits + clock.phase() + clamped_total_ns * kRate);
    }
}

} // namespace

int main() {
    check_constants();
    check_lcg_sequences();
    check_keep_drain();
    check_fraction_phase();
    check_rejection_preserves_state();
    check_fixed_clock_equivalence();
    std::puts("budget_contract: all checks passed");
    return 0;
}
