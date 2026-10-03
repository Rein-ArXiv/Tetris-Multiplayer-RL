#ifndef STUDY_NET_FIRST_ADMISSION_H
#define STUDY_NET_FIRST_ADMISSION_H

// First-admission phase router for the bounded teaching study.
//
// This object owns exactly one phase: the bytes and deadline for the first
// valid TYPE50 admission frame on a connection. It is a pure single-owner
// state machine. It never opens sockets, reads a clock, authenticates, assigns
// identity, or matches peers. The caller supplies received bytes, the current
// time, and the EOF observation, and later carries the transport across the
// handoff.
//
// Scope notes:
//   * The parser has fixed storage. Byte/ignored limits bound accepted phase
//     work; they are not a process-wide CPU or connection budget.
//   * Time is cooperative: the owner calls poll()/feed()/eof() with a
//     timestamp. There is no OS timer and no external cancellation.
//   * `route` only selects an admission path; it is not identity or auth.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/framing.h"

namespace study_net {

inline constexpr std::uint8_t kAdmissionType = 50;
inline constexpr std::size_t kAdmissionReadBytes = 16;
inline constexpr std::size_t kAdmissionByteLimit = 128;
inline constexpr std::size_t kAdmissionIgnoredLimit = 4;

enum class AdmissionRoute : std::uint8_t {
  queue = 1,
  create = 2,
  join = 3,
};

struct AdmissionRequest {
  AdmissionRoute route{AdmissionRoute::queue};
  std::array<std::uint8_t, 5> room{};
};

// Decode a TYPE50 admission frame. Payload layout:
//   [route:u8][room length:u8][room bytes]
//   * queue/create: payload size 2, room length 0.
//   * join:         payload size 7, room length 5, room bytes in [A-Z0-9].
// Every rejection leaves `out` untouched; all writes go to a local candidate.
// Bounds are checked before any payload byte is read.
inline bool decode_admission(const Frame& frame, AdmissionRequest& out) noexcept {
  if (frame.type != kAdmissionType) {
    return false;
  }
  if (frame.size != 2 && frame.size != 7) {
    return false;
  }
  const std::uint8_t route_raw = frame.payload[0];
  const std::uint8_t room_len = frame.payload[1];

  AdmissionRoute route = AdmissionRoute::queue;
  if (route_raw == static_cast<std::uint8_t>(AdmissionRoute::queue)) {
    route = AdmissionRoute::queue;
  } else if (route_raw == static_cast<std::uint8_t>(AdmissionRoute::create)) {
    route = AdmissionRoute::create;
  } else if (route_raw == static_cast<std::uint8_t>(AdmissionRoute::join)) {
    route = AdmissionRoute::join;
  } else {
    return false;
  }

  AdmissionRequest candidate{};
  candidate.route = route;

  if (route == AdmissionRoute::join) {
    if (frame.size != 7 || room_len != 5) {
      return false;
    }
    for (std::size_t i = 0; i < 5; ++i) {
      const std::uint8_t c = frame.payload[2 + i];
      const bool upper = c >= static_cast<std::uint8_t>('A') &&
                         c <= static_cast<std::uint8_t>('Z');
      const bool digit = c >= static_cast<std::uint8_t>('0') &&
                         c <= static_cast<std::uint8_t>('9');
      if (!upper && !digit) {
        return false;
      }
      candidate.room[i] = c;
    }
  } else {
    if (frame.size != 2 || room_len != 0) {
      return false;
    }
  }

  out = candidate;
  return true;
}

// Result of a poll/feed/eof step and the terminal reason for the phase.
enum class AdmissionState {
  waiting,
  routed,
  handed_off,
  timed_out,
  protocol_error,
  byte_limit,
  ignored_limit,
  peer_closed,
  truncated,
  clock_error,
};

// Ownership bundle produced by FirstAdmission::take(). The parser carries any
// buffered tail (a later frame or a partial one) that was left untouched.
struct AdmissionHandoff {
  AdmissionRequest request{};
  FrameParser parser{};
};

class FirstAdmission {
 public:
  // Create the phase router. A zero timeout can never be satisfied, so it is
  // rejected outright. There is no public default construction.
  static std::optional<FirstAdmission> create(std::uint64_t start,
                                              std::uint64_t timeout) noexcept {
    if (timeout == 0) {
      return std::nullopt;
    }
    return FirstAdmission(start, timeout);
  }

  // Observe time without extending the deadline. clock_error (now < last_) is transient: it is
  // returned but not stored, and no member changes. On a tie the deadline wins
  // over the byte limit.
  AdmissionState poll(std::uint64_t now) noexcept {
    if (now < last_) {
      return AdmissionState::clock_error;
    }
    last_ = now;
    if (state_ == AdmissionState::waiting) {
      if (now - start_ >= timeout_) {
        state_ = AdmissionState::timed_out;
      } else if (received_ >= kAdmissionByteLimit) {
        state_ = AdmissionState::byte_limit;
      }
    }
    return state_;
  }

  // Bytes the owner may hand to feed() next. The owner must poll() before the
  // next read; this is not a general-purpose buffer query.
  std::size_t read_capacity() const noexcept {
    if (state_ != AdmissionState::waiting) {
      return 0;
    }
    if (received_ >= kAdmissionByteLimit) {
      return 0;
    }
    const std::size_t remaining = kAdmissionByteLimit - received_;
    return remaining < kAdmissionReadBytes ? remaining : kAdmissionReadBytes;
  }

  // Append received bytes and drain complete frames.
  //   * count 0 is permitted and does not extend the deadline.
  //   * count > read_capacity() is a persistent byte_limit failure.
  //   * count > 0 with a null pointer is a protocol_error.
  //   * Unknown frame types are ignored up to kAdmissionIgnoredLimit; the
  //     fifth unknown frame fails with ignored_limit.
  //   * A TYPE50 frame that does not decode fails with protocol_error.
  //   * The first accepted TYPE50 frame routes immediately; draining stops and
  //     all later complete/partial bytes stay in the parser.
  //   * Partial or unknown bytes do not reset the deadline.
  AdmissionState feed(const std::uint8_t* data, std::size_t count,
                      std::uint64_t now) noexcept {
    const AdmissionState polled = poll(now);
    if (polled != AdmissionState::waiting) {
      return polled;
    }
    if (count > read_capacity()) {
      state_ = AdmissionState::byte_limit;
      return state_;
    }
    if (count > 0 && data == nullptr) {
      state_ = AdmissionState::protocol_error;
      return state_;
    }
    if (!parser_.append(data, count)) {
      state_ = AdmissionState::protocol_error;
      return state_;
    }
    received_ += count;

    while (state_ == AdmissionState::waiting) {
      Frame frame{};
      const ParseStatus status = parser_.next(frame);
      if (status == ParseStatus::error) {
        state_ = AdmissionState::protocol_error;
        break;
      }
      if (status == ParseStatus::need_more) {
        break;
      }
      if (frame.type != kAdmissionType) {
        ++ignored_;
        if (ignored_ > kAdmissionIgnoredLimit) {
          state_ = AdmissionState::ignored_limit;
          break;
        }
        continue;
      }
      AdmissionRequest candidate{};
      if (!decode_admission(frame, candidate)) {
        state_ = AdmissionState::protocol_error;
        break;
      }
      request_ = candidate;
      state_ = AdmissionState::routed;
      break;
    }

    if (state_ == AdmissionState::waiting &&
        received_ >= kAdmissionByteLimit) {
      state_ = AdmissionState::byte_limit;
    }
    return state_;
  }

  // Report transport EOF. While still waiting, pending bytes mean a truncated
  // stream; otherwise the peer closed cleanly. Any already-reached state,
  // including routed, is preserved so the owner can handle buffered bytes plus
  // EOF. The socket EOF itself is carried by the caller, not stored here.
  AdmissionState eof(std::uint64_t now) noexcept {
    const auto observed = poll(now);
    if (observed == AdmissionState::clock_error) return observed;
    if (state_ == AdmissionState::waiting) {
      state_ = parser_.pending_bytes() != 0 ? AdmissionState::truncated
                                            : AdmissionState::peer_closed;
    }
    return state_;
  }

  // Move the routed request and parser tail to `out`. Succeeds only from
  // routed and is logically once-only; on failure `out` is untouched. This
  // object does not own the transport, so no socket is transferred here.
  bool take(AdmissionHandoff& out) noexcept {
    if (state_ != AdmissionState::routed) {
      return false;
    }
    AdmissionHandoff candidate{request_, parser_};
    out = candidate;
    parser_ = FrameParser{};
    state_ = AdmissionState::handed_off;
    return true;
  }

 private:
  FirstAdmission(std::uint64_t start, std::uint64_t timeout) noexcept
      : start_(start), last_(start), timeout_(timeout) {}

  AdmissionState state_{AdmissionState::waiting};
  std::uint64_t start_{0};
  std::uint64_t last_{0};
  std::uint64_t timeout_{0};
  std::size_t received_{0};
  std::size_t ignored_{0};
  FrameParser parser_{};
  AdmissionRequest request_{};
};

}  // namespace study_net

#endif  // STUDY_NET_FIRST_ADMISSION_H
