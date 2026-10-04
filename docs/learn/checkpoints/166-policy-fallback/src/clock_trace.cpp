// CPU trace of scheduled ticks and unconsumed phase. No wall-clock waiting.
#include "timing/fixed_clock.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <vector>

namespace {

using study_timing::Batch;
using study_timing::FixedClock;

constexpr const char* kHeader = "frame,elapsed_ns,phase_before,ticks,phase_after";

void print_header() {
    std::puts(kHeader);
}

void emit_row(std::uint64_t frame,
              std::uint64_t elapsed_ns,
              std::uint64_t phase_before,
              unsigned ticks,
              std::uint64_t phase_after) {
    std::printf("%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,%" PRIu64 "\n",
                frame, elapsed_ns, phase_before, ticks, phase_after);
}

// 정수 ns 프레임 열을 새 clock에서 진행하며 프레임 경계를 한 줄씩 기록한다.
void run_ns_frames(const char* title, const std::vector<std::uint64_t>& frames, bool verbose = true) {
    std::printf("# %s\n", title);
    if (verbose) print_header();

    FixedClock clock;
    std::uint64_t total_ticks = 0;
    std::uint64_t frame = 1;
    for (const std::uint64_t elapsed_ns : frames) {
        const std::uint64_t phase_before = clock.phase();
        const Batch batch = clock.advance_ns(elapsed_ns);
        if (verbose) emit_row(frame, elapsed_ns, phase_before, batch.ticks, clock.phase());
        total_ticks += batch.ticks;
        ++frame;
    }
    std::printf("# total_ticks=%" PRIu64 ", final_phase=%" PRIu64 "\n",
                total_ticks, clock.phase());
}

// Display the known, nonnegative sub-nanosecond inputs after conversion.
void run_seconds_frames(const char* title, const std::vector<double>& frames) {
    std::printf("# %s\n", title);
    print_header();
    FixedClock clock;
    std::uint64_t frame = 1;
    for (const double seconds : frames) {
        const auto before = clock.phase();
        const auto batch = clock.advance_seconds(seconds);
        if (!batch) {
            std::fputs("invalid trace input\n", stderr);
            std::exit(1);
        }
        const auto ns = static_cast<std::uint64_t>(seconds * 1'000'000'000.0);
        emit_row(frame++, ns, before, batch->ticks, clock.phase());
    }
}

}  // namespace

int main() {
    // 5개 ns 프레임 합 50 ms -> 총 3틱, 최종 위상 0.
    run_ns_frames("five ns frames: 8/9/5/11/17 ms",
                  {8000000ULL, 9000000ULL, 5000000ULL, 11000000ULL, 17000000ULL});

    // 임계점: 16666666 ns까지 0틱, 다음 1 ns에서 1틱.
    run_ns_frames("threshold: 16666666 ns then 1 ns",
                  {16666666ULL, 1ULL});

    // 새 clock: 100 ms 열 번 -> 60틱, 위상 0.
    run_ns_frames("100 ms x 10",
                  std::vector<std::uint64_t>(10, 100000000ULL), false);

    // 새 clock: 10 ms 백 번 -> 60틱, 위상 0.
    run_ns_frames("10 ms x 100",
                  std::vector<std::uint64_t>(100, 10000000ULL), false);

    // seconds의 sub-ns 양자화 차이 예시.
    run_seconds_frames("0.6 ns x 2 (quantized ns shown)", {0.6e-9, 0.6e-9});
    run_seconds_frames("1.2 ns x 1 (fresh clock)", {1.2e-9});

    return 0;
}
