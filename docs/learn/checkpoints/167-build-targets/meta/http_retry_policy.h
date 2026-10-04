#ifndef STUDY_META_HTTP_RETRY_POLICY_H
#define STUDY_META_HTTP_RETRY_POLICY_H

#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace study_meta {

// What the caller should do after inspecting an HTTP response status.
// A retry decision is only a policy hint: it does not prove a server-side
// write failed, so callers must not treat it as confirmation of state.
enum class HttpAction {
  inspect_receipt,  // 200: response may carry a receipt worth examining.
  retry,            // 429 / 5xx: retry candidate under this API policy.
  stop,             // Anything else: do not retry automatically.
};

// 200 -> inspect_receipt; 429 and 500..599 -> retry; all others -> stop.
inline HttpAction http_action(int status) noexcept {
  if (status == 200) {
    return HttpAction::inspect_receipt;
  }
  if (status == 429 || (status >= 500 && status <= 599)) {
    return HttpAction::retry;
  }
  return HttpAction::stop;
}

// Backoff by completed attempts: 0 -> 0ms, 1 -> 100ms, >=2 -> 200ms.
// Branching avoids arithmetic, so no overflow is possible.
inline std::chrono::milliseconds retry_backoff(unsigned attempts) noexcept {
  if (attempts == 0) {
    return std::chrono::milliseconds{0};
  }
  if (attempts == 1) {
    return std::chrono::milliseconds{100};
  }
  return std::chrono::milliseconds{200};
}

// Parse a delta-seconds Retry-After hint. Accepts only a nonempty run of
// ASCII digits; rejects signs, whitespace, decimals, HTTP dates and overflow.
// 0 is allowed. Returns nullopt for unsupported hints so the caller stops
// automatic retry rather than silently retrying sooner.
inline std::optional<std::chrono::milliseconds> retry_after_seconds(
    const std::string& value) noexcept {
  if (value.empty()) {
    return std::nullopt;
  }
  for (char c : value) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
  }

  std::uint64_t seconds = 0;
  const char* const first = value.data();
  const char* const last = first + value.size();
  const std::from_chars_result parsed = std::from_chars(first, last, seconds);
  if (parsed.ec != std::errc{} || parsed.ptr != last) {
    return std::nullopt;
  }

  using rep = std::chrono::milliseconds::rep;
  constexpr std::uint64_t kMaxSeconds =
      static_cast<std::uint64_t>(std::numeric_limits<rep>::max()) / 1000;
  if (seconds > kMaxSeconds) {
    return std::nullopt;
  }

  return std::chrono::milliseconds{static_cast<rep>(seconds) * 1000};
}

}  // namespace study_meta

#endif  // STUDY_META_HTTP_RETRY_POLICY_H
