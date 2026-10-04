#ifndef STUDY_NET_HEARTBEAT_H
#define STUDY_NET_HEARTBEAT_H

// Application-level ping/pong heartbeat for the bounded study_net protocol.
//
// Wire types (see net/framing.h for the enclosing frame format):
//   * TYPE 30: ping  -- `are you alive?`
//   * TYPE 31: pong  -- `yes, and here is your token`
// Both carry exactly one 8-byte little-endian non-zero uint64 token.
//
// Heartbeat is a pure, single-owner state machine:
//   * It never allocates, throws, or touches a wall clock.
//   * All timestamps are logical, monotonically increasing milliseconds.
//     Subtraction only ever happens between ordered values (now >= last_now),
//     so counters cannot underflow.
//   * A backward clock step is reported as BeatHealth::clock_error and leaves
//     the object completely unchanged; status() aging still mutates state, so
//     even a rejected issue()/pong() may advance last_now/expired.
//   * issue() models admission onto the send path, not delivery to the OS.
//
// Timing is supplied by the caller; the threading wrapper is expected to
// validate it with BeatTiming::valid() before construction.

#include <cstdint>
#include <limits>

#include "net/byte_codec.h"
#include "net/framing.h"

namespace study_net {

// Application message types for the heartbeat exchange.
inline constexpr std::uint8_t kBeatTypePing = 30;
inline constexpr std::uint8_t kBeatTypePong = 31;

// The token is always exactly 8 bytes: one little-endian uint64.
inline constexpr std::size_t kBeatPayloadBytes = 8;

static_assert(kBeatPayloadBytes <= kMaxPayloadBytes,
              "heartbeat payload must fit in a study_net frame");

// Encode a ping (pong == false) or pong (pong == true) carrying `token`.
// A zero token is not a valid probe and is rejected. On any failure `out` is
// left untouched: the frame is assembled in a local candidate first.
inline bool encode_beat(bool pong, std::uint64_t token, Frame& out) noexcept {
  if (token == 0) {
    return false;
  }
  Frame candidate{};
  candidate.type = pong ? kBeatTypePong : kBeatTypePing;
  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  if (!writer.u64(token)) {
    return false;
  }
  candidate.size = kBeatPayloadBytes;
  out = candidate;
  return true;
}

// Decode a heartbeat frame. Type, exact payload size and the non-zero token
// are all validated before `pong`/`token` are written, so a rejected frame
// leaves both outputs unchanged.
inline bool decode_beat(const Frame& frame, bool& pong,
                        std::uint64_t& token) noexcept {
  if (frame.type != kBeatTypePing && frame.type != kBeatTypePong) {
    return false;
  }
  if (frame.size != kBeatPayloadBytes) {
    return false;
  }
  ByteReader reader(frame.payload.data(), frame.size);
  std::uint64_t candidate = 0;
  if (!reader.u64(candidate)) {
    return false;
  }
  if (candidate == 0) {
    return false;
  }
  pong = (frame.type == kBeatTypePong);
  token = candidate;
  return true;
}

// Liveness thresholds in logical milliseconds. A missing pong is tolerated up
// to `suspect` and expires by policy at `timeout`; the bands are non-overlapping
// because 0 < interval < suspect < timeout.
struct BeatTiming {
  std::uint64_t interval = 1000;
  std::uint64_t suspect = 2000;
  std::uint64_t timeout = 3000;

  bool valid() const noexcept {
    return interval > 0 && interval < suspect && suspect < timeout;
  }
};

enum class BeatHealth {
  waiting,      // no pong confirmed yet, still inside the suspect window
  healthy,      // a pong was confirmed and the last confirmation is recent
  suspect,      // no confirmation within `suspect`
  expired,      // no confirmation within `timeout` (latched)
  clock_error,  // time moved backwards; no state was modified
};

// Single-owner heartbeat state machine. Non-copyable to keep ownership clear.
class Heartbeat {
 public:
  Heartbeat(std::uint64_t start, BeatTiming timing = {}) noexcept
      : timing_(timing),
        last_now_(start),
        last_good_(start),
        last_sent_(start) {}

  Heartbeat(const Heartbeat&) = delete;
  Heartbeat& operator=(const Heartbeat&) = delete;

  // Age the machine to `now` and report health.
  //  * now < last_now: clock_error, object untouched.
  //  * otherwise last_now advances; `expired` latches once reached.
  BeatHealth status(std::uint64_t now) noexcept {
    if (now < last_now_) {
      return BeatHealth::clock_error;
    }
    last_now_ = now;
    if (expired_) {
      return BeatHealth::expired;
    }
    const std::uint64_t since_good = now - last_good_;
    if (since_good >= timing_.timeout) {
      expired_ = true;
      return BeatHealth::expired;
    }
    if (since_good >= timing_.suspect) {
      return BeatHealth::suspect;
    }
    return confirmed_ ? BeatHealth::healthy : BeatHealth::waiting;
  }

  // Admit one probe at `now`. Returns false (with `out` untouched) when the
  // link is expired/clock_error, a probe is already pending, the token space
  // is exhausted, or `interval` has not elapsed since the last send. On
  // success `out` receives the non-zero token and the probe becomes pending.
  bool issue(std::uint64_t now, std::uint64_t& out) noexcept {
    if (!usable(status(now))) {
      return false;
    }
    if (pending_ != 0) {
      return false;
    }
    if (next_token_ == 0) {
      return false;
    }
    if (sent_ && (now - last_sent_) < timing_.interval) {
      return false;
    }
    const std::uint64_t token = next_token_;
    pending_ = token;
    last_sent_ = now;
    sent_ = true;
    next_token_ = (token == (std::numeric_limits<std::uint64_t>::max)())
                      ? std::uint64_t{0}
                      : token + 1;
    out = token;
    return true;
  }

  // Confirm the outstanding probe. Only the exact pending token renews
  // liveness; a pong at or after the timeout is rejected because status() has
  // already latched `expired`. Unmatched or replayed tokens never renew.
  bool pong(std::uint64_t token, std::uint64_t now) noexcept {
    if (!usable(status(now))) {
      return false;
    }
    if (pending_ == 0 || token != pending_) {
      return false;
    }
    pending_ = 0;
    last_good_ = now;
    confirmed_ = true;
    return true;
  }

 private:
  static bool usable(BeatHealth health) noexcept {
    return health != BeatHealth::expired && health != BeatHealth::clock_error;
  }

  BeatTiming timing_{};
  std::uint64_t last_now_{0};
  std::uint64_t last_good_{0};
  std::uint64_t last_sent_{0};
  bool confirmed_{false};
  bool sent_{false};
  bool expired_{false};
  std::uint64_t pending_ = 0; // Zero is reserved on wire: no pending request.
  std::uint64_t next_token_{1};
};

}  // namespace study_net

#endif  // STUDY_NET_HEARTBEAT_H