#pragma once
// CPU-only policy comparison: schedules up to six ticks per call.
// The caller runs the rules; this clock tracks the unconsumed time.
#include <cstdint>
#include <optional>

namespace study_budget {

// 시간 처리 정책 선택.
enum class Policy {
    clamp_elapsed,    // 입력을 100ms 로 자르고 나머지를 clamped_ns 로 보고
    keep_backlog,     // 전체 틱 + fraction 모두 보존
    discard_backlog,  // 전체 틱은 버리고 fraction 만 보존
};

// 한 번의 advance_ns() 결과 보고. 모든 필드는 정수이며 단위를 주석으로 명시한다.
struct Report {
    unsigned ticks = 0;              // 이번 호출에서 발행한 틱 수 [tick]
    std::uint64_t pending_ticks = 0; // 남아 있는 "전체" 틱 수 [tick]
    std::uint64_t phase = 0;         // 남은 sub-tick 크레딧 [unit, 1e9 unit == 1 tick]
    std::uint64_t clamped_ns = 0;    // 정책에 의해 잘려나간 입력 시간 [ns]
    std::uint64_t dropped_ticks = 0; // 정책에 의해 버려진 "전체" 틱 수 [tick]
};

class CatchUpClock {
public:
    // 규격 상수. 기존 FixedClock 과 독립적으로 유지한다.
    static constexpr std::uint64_t rate = 60;                  // [tick/s]
    static constexpr std::uint64_t units = 1'000'000'000;      // [unit/tick]
    static constexpr std::uint64_t max_frame_ns = 100'000'000; // [ns] == 100ms
    static constexpr unsigned budget = 6;                      // 한 호출 최대 발행 틱 [tick]

    static_assert(max_frame_ns * rate == static_cast<std::uint64_t>(budget) * units,
                  "clamp policy must fit exactly in the per-call tick budget");

    explicit CatchUpClock(Policy policy) noexcept : policy_(policy) {}

    // 정수 나노초를 받아 정책에 따라 틱을 발행한다.
    // 유효하지 않은 입력(잘못된 enum, overflow)이면 nullopt 를 돌려주고
    // 내부 상태(credits_)를 원본 그대로 보존한다.
    std::optional<Report> advance_ns(std::uint64_t elapsed_ns) noexcept {
        const Policy policy = policy_;
        const bool valid = policy == Policy::clamp_elapsed ||
                           policy == Policy::keep_backlog ||
                           policy == Policy::discard_backlog;
        if (!valid) return std::nullopt; // 잘못된 enum 값 -> 상태 보존

        // 1) 정책별로 수락할 입력 시간과 잘려나간 시간을 먼저 확정한다.
        std::uint64_t accepted_ns = elapsed_ns;
        std::uint64_t clamped_ns = 0;
        if (policy == Policy::clamp_elapsed && elapsed_ns > max_frame_ns) {
            clamped_ns = elapsed_ns - max_frame_ns; // clamped_ns 는 여기서 한 번만 계산
            accepted_ns = max_frame_ns;
        }

        // 2) 곱셈 overflow 검사는 total 계산 전에 수행한다(원본 상태 보존).
        const std::uint64_t room = UINT64_MAX - credits_;
        if (accepted_ns > room / rate) return std::nullopt;

        // 3) 모든 값을 local 로 계산한다(멤버 부분 변경 금지).
        const std::uint64_t total = credits_ + accepted_ns * rate; // [unit]
        const std::uint64_t whole = total / units;                 // [tick]
        const unsigned issued = whole > budget ? budget : static_cast<unsigned>(whole);

        // 4) 정책별 잔여/폐기를 계산한다. clamped_ns 와 dropped_ticks 는
        //    서로 다른 경로에서만 만들어지므로 중복 계산이 없다.
        std::uint64_t next_credits = 0;
        std::uint64_t pending_ticks = 0;
        std::uint64_t dropped_ticks = 0;
        if (policy == Policy::clamp_elapsed) {
            // backlog 를 쌓지 않는다: 발행하지 못한 전체 틱은 존재하지 않는다.
            next_credits = total % units;
        } else if (policy == Policy::keep_backlog) {
            // 발행하고 남은 전체 틱 + fraction 을 그대로 보존한다.
            next_credits = total - static_cast<std::uint64_t>(issued) * units;
            pending_ticks = next_credits / units;
        } else { // discard_backlog
            // 남은 전체 틱은 버리고 fraction 만 보존한다.
            dropped_ticks = whole - issued;
            next_credits = total % units; // 곱셈 없이 바로 다음 credits 계산
        }

        credits_ = next_credits; // 마지막에 한 번만 대입
        return Report{issued, pending_ticks, next_credits % units, clamped_ns,
                      dropped_ticks};
    }

    // 아직 실행하지 않은 "전체" 틱 수 [tick].
    std::uint64_t backlog_ticks() const noexcept { return credits_ / units; }
    // 아직 소비하지 않은 한 틱 미만의 위상 [unit].
    std::uint64_t phase() const noexcept { return credits_ % units; }

private:
    Policy policy_;
    std::uint64_t credits_ = 0; // 전체 틱 + fraction 을 담는 정수 크레딧 [unit]
};

} // namespace study_budget
