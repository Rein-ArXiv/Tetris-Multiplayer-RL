#ifndef STUDY_NET_ROUND_PROTOCOL_H
#define STUDY_NET_ROUND_PROTOCOL_H

// Educational round-tagged input envelope for the study_net framing layer.
// Local framing only: it is NOT the root INPUT4 message and shares no wire
// encoding with it. Reuses InputBatch mask validation, adds a round tag.
//
// Payload (little-endian), exactly 14 + count bytes:
//   [0..7]   uint64 round        (nonzero)
//   [8..11]  uint32 first_tick
//   [12..13] uint16 count        (1..kMaxBatchInputs)
//   [14..]   count input-mask bytes
// count in 1..16 => payload 15..30 bytes, within the 32-byte cap.
//
// The round field is framing metadata: neither codec authenticates it nor
// agrees a seed with a peer. Any failure leaves the caller output untouched.

#include <cstddef>
#include <cstdint>

#include "net/input_codec.h"

namespace study_net {

inline constexpr std::uint8_t kRoundInputType = 41;
inline constexpr std::size_t kRoundHeaderBytes = 14;  // round + tick + count

struct RoundBatch {
  std::uint64_t round{0};
  InputBatch inputs{};
};

inline bool encode_round_input(const RoundBatch& batch, Frame& out) noexcept {
  if (batch.round == 0 || !valid_input_batch(batch.inputs)) {
    return false;
  }

  // Fill a local candidate; publish only on whole-message success.
  Frame candidate{};
  candidate.type = kRoundInputType;

  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  if (!writer.u64(batch.round)) {
    return false;
  }
  if (!writer.u32(batch.inputs.first_tick)) {
    return false;
  }
  if (!writer.u16(static_cast<std::uint16_t>(batch.inputs.count))) {
    return false;
  }
  for (std::size_t i = 0; i < batch.inputs.count; ++i) {
    if (!writer.u8(batch.inputs.masks[i])) {
      return false;
    }
  }

  candidate.size = writer.position();
  out = candidate;
  return true;
}

inline bool decode_round_input(const Frame& frame, RoundBatch& out) noexcept {
  if (frame.type != kRoundInputType) {
    return false;
  }
  // Reject an impossible declared size before building a reader.
  if (frame.size > frame.payload.size() || frame.size < kRoundHeaderBytes) {
    return false;
  }

  ByteReader reader(frame.payload.data(), frame.size);

  std::uint64_t round = 0;
  if (!reader.u64(round) || round == 0) {
    return false;
  }

  std::uint32_t first_tick = 0;
  if (!reader.u32(first_tick)) {
    return false;
  }

  std::uint16_t count16 = 0;
  if (!reader.u16(count16)) {
    return false;
  }

  // Narrowing is not range checking: validate before using as a count.
  if (count16 < 1 || count16 > kMaxBatchInputs) {
    return false;
  }

  const std::size_t count = static_cast<std::size_t>(count16);

  // Exact size policy: exactly count mask bytes must remain.
  if (reader.remaining() != count) {
    return false;
  }

  // Build and validate a local candidate before publishing.
  RoundBatch candidate{};
  candidate.round = round;
  candidate.inputs.first_tick = first_tick;
  candidate.inputs.count = count;
  for (std::size_t i = 0; i < count; ++i) {
    if (!reader.u8(candidate.inputs.masks[i])) {
      return false;
    }
  }

  if (!reader.at_end() || !valid_input_batch(candidate.inputs)) {
    return false;
  }

  out = candidate;
  return true;
}

}  // namespace study_net

#endif  // STUDY_NET_ROUND_PROTOCOL_H
