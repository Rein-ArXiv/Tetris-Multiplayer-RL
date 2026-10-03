// 45차시 계약 테스트: FixedClock의 ns -> 60 Hz 정수 위상 회계를 독립 참조로 검증한다.
//
// Release(NDEBUG)에서도 검사가 살아 있도록 assert 대신 CHECK를 사용한다.
// 독립 참조는 "수락된 ns 누적합 x 60"의 몫(틱)과 나머지(위상)이며,
// 기존 clock을 반복 호출한 출력을 기대값으로 쓰지 않는다.
#include "timing/fixed_clock.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr std::uint64_t kRate = 60;
constexpr std::uint64_t kUnits = 1'000'000'000;
constexpr std::uint64_t kMaxFrameNs = 100'000'000;

[[noreturn]] void check_failed(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", expression, file, line);
    std::exit(1);
}

#define CHECK(condition) do { if (!(condition)) { check_failed(#condition, __FILE__, __LINE__); } } while (false)

// 독립 참조: 수락된 ns 누적합 x rate의 몫(틱)과 나머지(위상).
// 누적합은 uint64 범위 안에 있음을 전제로 한다.
struct Reference {
    std::uint64_t ticks;
    std::uint64_t phase;
};

Reference reference_of(std::uint64_t accepted_ns_total) {
    const std::uint64_t scaled = accepted_ns_total * kRate;
    return Reference{scaled / kUnits, scaled % kUnits};
}

std::pair<std::uint64_t, std::uint64_t> run_partition(
    const std::vector<std::uint64_t>& frames) {
    study_timing::FixedClock clock;
    std::uint64_t ticks = 0;
    for (const std::uint64_t ns : frames) {
        const study_timing::Batch batch = clock.advance_ns(ns);
        CHECK(!batch.clamped);
        ticks += batch.ticks;
    }
    return {ticks, clock.phase()};
}

std::uint32_t lcg_next(std::uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return state;
}

void test_zero_advance_preserves_phase() {
    study_timing::FixedClock clock;
    const study_timing::Batch seed = clock.advance_ns(16666666ULL);
    CHECK(!seed.clamped);
    const std::uint64_t phase_before = clock.phase();
    CHECK(phase_before == 16666666ULL * kRate);

    const study_timing::Batch zero = clock.advance_ns(0ULL);
    CHECK(zero.ticks == 0);
    CHECK(!zero.clamped);
    CHECK(clock.phase() == phase_before);
}

void test_threshold_ns() {
    study_timing::FixedClock clock;
    const study_timing::Batch first = clock.advance_ns(16666666ULL);
    CHECK(!first.clamped);
    CHECK(first.ticks == 0);
    CHECK(clock.phase() == 999999960ULL);

    const study_timing::Batch second = clock.advance_ns(1ULL);
    CHECK(!second.clamped);

    const Reference reference = reference_of(16666666ULL + 1ULL);
    CHECK(reference.ticks == 1);
    CHECK(reference.phase == 20);
    CHECK(second.ticks == reference.ticks);
    CHECK(clock.phase() == reference.phase);
}

void test_lcg_accumulation() {
    study_timing::FixedClock clock;
    std::uint32_t state = 0x12345678U;
    std::uint64_t accepted_total = 0;
    std::uint64_t ticks_total = 0;

    for (int i = 0; i < 10000; ++i) {
        const std::uint64_t ns = lcg_next(state) % 100000001ULL;
        const study_timing::Batch batch = clock.advance_ns(ns);
        CHECK(!batch.clamped);
        accepted_total += ns;
        ticks_total += batch.ticks;
        CHECK(clock.phase() < kUnits);
        const Reference reference = reference_of(accepted_total);
        CHECK(ticks_total == reference.ticks);
        CHECK(clock.phase() == reference.phase);
    }

    CHECK(accepted_total <= 10000ULL * kMaxFrameNs);
    const Reference reference = reference_of(accepted_total);
    CHECK(ticks_total == reference.ticks);
    CHECK(clock.phase() == reference.phase);
}

void test_partition_equivalence() {
    const std::uint64_t total_ns = 50000000ULL;  // 50 ms, 단일 프레임도 클램프 없음.
    const Reference reference = reference_of(total_ns);
    CHECK(reference.ticks == 3);
    CHECK(reference.phase == 0);

    const std::vector<std::vector<std::uint64_t>> partitions = {
        {50000000ULL},
        {25000000ULL, 25000000ULL},
        {10000000ULL, 10000000ULL, 10000000ULL, 10000000ULL, 10000000ULL},
        {8000000ULL, 9000000ULL, 5000000ULL, 11000000ULL, 17000000ULL},
        std::vector<std::uint64_t>(50, 1000000ULL),
    };

    for (const std::vector<std::uint64_t>& frames : partitions) {
        std::uint64_t sum = 0;
        for (const std::uint64_t ns : frames) {
            sum += ns;
        }
        CHECK(sum == total_ns);

        const std::pair<std::uint64_t, std::uint64_t> result = run_partition(frames);
        CHECK(result.first == reference.ticks);
        CHECK(result.second == reference.phase);
    }
}

void test_clamp_breaks_naive_equivalence() {
    study_timing::FixedClock whole;
    const std::optional<study_timing::Batch> one_second = whole.advance_seconds(1.0);
    CHECK(one_second.has_value());
    CHECK(one_second->clamped);
    CHECK(one_second->ticks == 6);  // 1 s는 0.1 s로 클램프 -> 6틱.
    CHECK(whole.phase() == 0);

    study_timing::FixedClock split;
    std::uint64_t ticks = 0;
    for (int i = 0; i < 10; ++i) {
        const study_timing::Batch batch = split.advance_ns(100000000ULL);  // 100 ms
        CHECK(!batch.clamped);
        ticks += batch.ticks;
    }
    CHECK(ticks == 60);
    CHECK(split.phase() == 0);

    // 클램프가 끼면 조건 없는 분할 동등성은 성립하지 않는다.
    CHECK(one_second->ticks != ticks);
}

void test_seconds_subns_quantization() {
    study_timing::FixedClock halved;
    const std::optional<study_timing::Batch> first = halved.advance_seconds(0.6e-9);
    CHECK(first.has_value());
    CHECK(!first->clamped);
    CHECK(first->ticks == 0);
    CHECK(halved.phase() == 0);

    const std::optional<study_timing::Batch> second = halved.advance_seconds(0.6e-9);
    CHECK(second.has_value());
    CHECK(second->ticks == 0);
    CHECK(halved.phase() == 0);

    study_timing::FixedClock single;
    const std::optional<study_timing::Batch> combined = single.advance_seconds(1.2e-9);
    CHECK(combined.has_value());
    CHECK(!combined->clamped);
    CHECK(combined->ticks == 0);
    CHECK(single.phase() == reference_of(1ULL).phase);
    CHECK(single.phase() == 60);

    // 같은 수학적 합(1.2 ns)이라도 sub-ns 양자화로 위상이 다르다.
    CHECK(halved.phase() != single.phase());
}

void test_invalid_seconds_preserve_phase() {
    study_timing::FixedClock clock;
    const study_timing::Batch seed = clock.advance_ns(16666666ULL);
    CHECK(!seed.clamped);
    const std::uint64_t phase_before = clock.phase();
    CHECK(phase_before == 999999960ULL);

    const double invalid[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -0.001,
    };
    for (const double seconds : invalid) {
        const std::optional<study_timing::Batch> batch = clock.advance_seconds(seconds);
        CHECK(!batch.has_value());
        CHECK(clock.phase() == phase_before);
    }

    // 거부 뒤에도 위상이 보존되어 정상 진행이 이어진다.
    const study_timing::Batch next = clock.advance_ns(1ULL);
    CHECK(next.ticks == 1);
    CHECK(clock.phase() == 20);
}

}  // namespace

int main() {
    test_zero_advance_preserves_phase();
    test_threshold_ns();
    test_lcg_accumulation();
    test_partition_equivalence();
    test_clamp_breaks_naive_equivalence();
    test_seconds_subns_quantization();
    test_invalid_seconds_preserve_phase();

    std::puts("clock_accounting: all checks passed");
    return 0;
}
