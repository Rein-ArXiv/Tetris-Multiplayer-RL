#ifndef STUDY_NET_NET_DEADLINE_WAIT_H_
#define STUDY_NET_NET_DEADLINE_WAIT_H_

// Deadline-to-timeout conversion with saturation and upward rounding.
//
// This header performs no clock reads, no blocking, and no I/O: callers own
// the "now" sample, which keeps the conversion pure and easy to test with
// synthetic time points.

#include <chrono>

namespace study_net {

// Largest timeout we are willing to report, in whole milliseconds.
// 0x3fffffff is about 12.4 days on the supported 32-bit-or-wider int targets.
inline constexpr int kMaxTimerWaitMs = 0x3fffffff;

// Translate an absolute deadline into a relative millisecond timeout.
// Returns 0 when the deadline has already passed; otherwise returns a
// positive value no larger than kMaxTimerWaitMs.
inline int deadline_wait_ms(std::chrono::steady_clock::time_point now,
                            std::chrono::steady_clock::time_point when) {
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = Clock::duration;

  // The cap as a Clock::duration. On the supported target clocks a
  // millisecond is represented exactly (the tick period evenly divides 1 ms),
  // so this round-trips to the same whole-millisecond count.
  constexpr Duration cap_duration =
      std::chrono::duration_cast<Duration>(std::chrono::milliseconds(kMaxTimerWaitMs));
  static_assert(
      std::chrono::duration_cast<std::chrono::milliseconds>(cap_duration).count() == kMaxTimerWaitMs,
      "target clock cannot represent the millisecond cap exactly");

  if (when <= now) {
    return 0;  // Deadline already expired: nothing to wait for.
  }

  // Clamp before subtracting. Computing when - now first could overflow a
  // signed duration for far-future deadlines. The comparison below is itself
  // overflow-free, because cap_duration is non-negative so max()-cap_duration
  // is a valid time point.
  if (now <= TimePoint::max() - cap_duration && when >= now + cap_duration) {
    return kMaxTimerWaitMs;
  }

  // Reaching here proves the gap is representable and strictly smaller than
  // cap_duration, so this subtraction cannot overflow. Round up to the next
  // whole millisecond: a positive sub-millisecond gap would otherwise
  // truncate to 0, and a zero timeout is the classic cause of a busy spin
  // when a caller loops on a deadline. The result is the requested wait, not
  // a promise of the actual scheduler wake time.
  const Duration remaining = when - now;
  return static_cast<int>(
      std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
}

}  // namespace study_net

#endif  // STUDY_NET_NET_DEADLINE_WAIT_H_
