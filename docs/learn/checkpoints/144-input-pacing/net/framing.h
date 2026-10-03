#ifndef STUDY_NET_FRAMING_H
#define STUDY_NET_FRAMING_H

// Framing for the bounded teaching study:
//   [LEN:u16 little endian][TYPE:u8][PAYLOAD:LEN-1]   (no checksum)
// LEN counts TYPE + PAYLOAD, not the 2-byte header. LEN is in [1, 33].
// This wire format is deliberately NOT compatible with the root game protocol.
//
// Responsibilities are kept separate on purpose:
//   * FrameParser  byte stream -> whole frames (partial headers/bodies and
//                  several frames per read; retains an incomplete tail).
//   * encode_frame Frame -> one encoded frame (bytes + size).
// Application type validation is left to the caller.
//
// The caller must drain until ParseStatus::need_more before the next read.
// An incomplete tail is always < kMaxFrameBytes, so a bounded read of at most
// 16 bytes keeps tail_read + 16 <= 34 + 16 = 50 < kReceiveCapacity (64).
// The length cap alone does not bound waiting time: a peer can stay silent.

#include <array>
#include <cstddef>
#include <cstdint>

#include "net/byte_buffer.h"

namespace study_net {

inline constexpr std::size_t kMaxPayloadBytes = 32;
inline constexpr std::size_t kHeaderBytes = 2;
inline constexpr std::size_t kMaxFrameBytes = 35;
inline constexpr std::size_t kReceiveCapacity = 64;
inline constexpr std::uint16_t kMaxLength = 33;

static_assert(kMaxFrameBytes == kHeaderBytes + 1 + kMaxPayloadBytes,
              "frame size must equal header + type + max payload");
static_assert(kReceiveCapacity > kMaxFrameBytes,
              "receive buffer must hold a whole maximum frame");

// One application frame. `size` is the payload length (0..kMaxPayloadBytes).
// A freshly decoded frame has a zeroed unused payload tail.
struct Frame {
  std::uint8_t type{};
  std::array<std::uint8_t, kMaxPayloadBytes> payload{};
  std::size_t size{};
};

// One encoded frame. `size` is the total encoded length (3 + payload length).
struct EncodedFrame {
  std::array<std::uint8_t, kMaxFrameBytes> bytes{};
  std::size_t size{};
};

// Encode `frame` into `out`. On failure (payload too large) `out` is untouched:
// every write goes to a local candidate assigned only on success. No struct
// memcpy, no allocations, explicit little-endian length bytes.
inline bool encode_frame(const Frame& frame, EncodedFrame& out) noexcept {
  if (frame.size > kMaxPayloadBytes) {
    return false;
  }
  EncodedFrame candidate{};
  const std::uint16_t len =
      static_cast<std::uint16_t>(frame.size + std::size_t{1});
  candidate.bytes[0] = static_cast<std::uint8_t>(len & 0xFFu);
  candidate.bytes[1] = static_cast<std::uint8_t>((len >> 8) & 0xFFu);
  candidate.bytes[2] = frame.type;
  for (std::size_t i = 0; i < frame.size; ++i) {
    candidate.bytes[kHeaderBytes + 1 + i] = frame.payload[i];
  }
  candidate.size = kHeaderBytes + 1 + frame.size;
  out = candidate;
  return true;
}

enum class ParseStatus { need_more, frame, error };

// Bounded stream-to-frame parser owning a ByteBuffer<kReceiveCapacity>.
// A failure is sticky until a fresh parser is constructed; there is no reset
// or recovery path on purpose.
class FrameParser {
 public:
  FrameParser() noexcept = default;

  // Append received bytes.
  //  * An empty append is valid while not failed.
  //  * If the bytes do not fit, failure latches and the buffer is cleared.
  //  * Once failed, every further append returns false and stores nothing.
  bool append(const std::uint8_t* data, std::size_t count) noexcept {
    if (failed_) {
      return false;
    }
    if (!buffer_.append(data, count)) {
      failed_ = true;
      (void)buffer_.consume_front(buffer_.size());
      return false;
    }
    return true;
  }

  // Extract the next frame.
  //  * need_more: not enough bytes yet; `out` untouched, nothing consumed.
  //  * frame:     complete frame copied to `out` and consumed exactly.
  //  * error:     malformed length (0 or > kMaxLength) or latched failure;
  //               buffer cleared and `out` untouched. A bad length is rejected
  //               as soon as the 2 header bytes are present.
  ParseStatus next(Frame& out) noexcept {
    if (failed_) {
      return ParseStatus::error;
    }
    if (buffer_.size() < kHeaderBytes) {
      return ParseStatus::need_more;
    }
    const std::uint8_t* p = buffer_.data();
    const std::uint16_t len = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        (static_cast<std::uint16_t>(p[1]) << 8));
    if (len < 1u || len > kMaxLength) {
      failed_ = true;
      (void)buffer_.consume_front(buffer_.size());
      return ParseStatus::error;
    }
    const std::size_t total = kHeaderBytes + static_cast<std::size_t>(len);
    if (buffer_.size() < total) {
      return ParseStatus::need_more;
    }
    Frame candidate{};
    candidate.type = p[kHeaderBytes];
    for (std::size_t i = 1; i < len; ++i) {
      candidate.payload[i - 1] = p[kHeaderBytes + i];
    }
    candidate.size = static_cast<std::size_t>(len) - 1;
    (void)buffer_.consume_front(total);
    out = candidate;
    return ParseStatus::frame;
  }

  // Unconsumed bytes. Only after draining to need_more is this an incomplete
  // tail; at that point EOF with pending_bytes() != 0 means truncation.
  std::size_t pending_bytes() const noexcept { return buffer_.size(); }

  bool failed() const noexcept { return failed_; }

 private:
  ByteBuffer<kReceiveCapacity> buffer_{};
  bool failed_{false};
};

}  // namespace study_net

#endif  // STUDY_NET_FRAMING_H
