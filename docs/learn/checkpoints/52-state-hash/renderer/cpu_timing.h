// renderer/cpu_timing.h
//
// Wall-clock CPU stage timing helper.
//
// IMPORTANT: every value produced here measures *wall-clock elapsed time*
// around CPU-side calls only. It is NOT the GPU execution duration of the
// submitted work, and it is NOT the time the frame is actually displayed on
// the monitor. Do not present these numbers as rendering or completion time.

#ifndef STUDY_TIMING_RENDERER_CPU_TIMING_H
#define STUDY_TIMING_RENDERER_CPU_TIMING_H

#include <cstdint>   // std::uint64_t
#include <optional>  // std::optional

namespace study_timing {

// Elapsed wall-clock time spent in each CPU-side stage, in milliseconds.
struct CpuStages {
    double submit_ms;   // wall-clock time around the submit call
    double sync_ms;     // wall-clock time around the sync/wait call
    double present_ms;  // wall-clock time around the present/swap call
};

// Convert a non-negative tick delta to milliseconds; frequency must be nonzero.
//
// The subtraction must happen on the integer tick values *before* any
// conversion to double: two timestamps both near UINT64_MAX can still have a
// small, meaningful difference that is lost if the large timestamps are rounded, so
// (double)a - (double)b loses it while the exact integer (a - b) keeps it.
inline double ticks_to_ms(std::uint64_t ticks, std::uint64_t frequency) noexcept {
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency);
}

// Summarize the CPU-side stages of a single frame.
//
// Timestamps are raw counter values taken from the same clock and are expected
// to be non-decreasing in this order:
//     begin <= after_submit <= after_sync <= after_present
// frequency is that clock's tick rate in Hz (ticks per second).
//
// Returns std::nullopt when the measurement is not usable:
//   * frequency == 0  (ticks cannot be converted to time)
//   * any timestamp decreases (out-of-order or corrupted capture)
//
// On success each field is the wall-clock elapsed time of its stage, i.e. the
// difference between the two timestamps bracketing that CPU call. No totals or
// averages are computed.
inline std::optional<CpuStages> summarize(std::uint64_t begin,
                                          std::uint64_t after_submit,
                                          std::uint64_t after_sync,
                                          std::uint64_t after_present,
                                          std::uint64_t frequency) noexcept {
    if (frequency == 0) {
        return std::nullopt;
    }

    // Reject any decreasing timestamp before doing arithmetic, so every delta
    // computed below is guaranteed to be non-negative.
    if (after_submit < begin || after_sync < after_submit || after_present < after_sync) {
        return std::nullopt;
    }

    // Integer subtraction first (exact even near UINT64_MAX), then to double, then to ms.
    CpuStages stages{};
    stages.submit_ms = ticks_to_ms(after_submit - begin, frequency);
    stages.sync_ms = ticks_to_ms(after_sync - after_submit, frequency);
    stages.present_ms = ticks_to_ms(after_present - after_sync, frequency);
    return stages;
}

}  // namespace study_timing

#endif  // STUDY_TIMING_RENDERER_CPU_TIMING_H