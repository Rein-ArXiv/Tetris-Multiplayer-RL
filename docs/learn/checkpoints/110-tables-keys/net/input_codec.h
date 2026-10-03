#ifndef STUDY_NET_INPUT_CODEC_H
#define STUDY_NET_INPUT_CODEC_H

// Educational codec for the diagnostic input-batch envelope in study_net.
//
// Framing type 1 is a diagnostic probe request envelope, not the root protocol
// INPUT4 message; a response of type 2 carries the identical payload layout.
// Payload layout (little-endian):
//   [0..3]  uint32 first_tick
//   [4..5]  uint16 count        (1..kMaxBatchInputs)
//   [6..]   count bytes of input masks
// Total payload size is exactly kInputHeaderBytes + count; any missing or extra
// trailing byte makes the whole payload invalid.
//
// Transactional output: an encoder/decoder failure at any field or at the final
// structural check leaves the caller-supplied output object exactly as it was.

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "net/byte_codec.h"
#include "net/framing.h"
#include "simulation/input_mask.h"

namespace study_net {

constexpr size_t kMaxBatchInputs = 16;
constexpr size_t kInputHeaderBytes = 6;  // uint32 first_tick + uint16 count

struct InputBatch {
  uint32_t first_tick{};
  std::array<uint8_t, kMaxBatchInputs> masks{};
  size_t count{};
};

// Message-local validation. This checks the batch's own consistency and the
// per-tick input-mask domain; it deliberately makes no network-distance or
// connection-level judgements, which belong to a higher layer.
inline bool valid_input_batch(const InputBatch& batch) noexcept {
  if (batch.count < 1 || batch.count > kMaxBatchInputs) {
    return false;
  }

  // Verify the tick span fits before adding any ticks, so first_tick + count - 1
  // can never wrap around the uint32 domain.
  if ((std::numeric_limits<uint32_t>::max)() - batch.first_tick <
      static_cast<uint32_t>(batch.count - 1)) {
    return false;
  }

  for (size_t i = 0; i < batch.count; ++i) {
    if (!study_input::valid(batch.masks[i])) {
      return false;
    }
  }
  return true;
}

inline bool encode_input_payload(const InputBatch& batch, Frame& out) noexcept {
  if (!valid_input_batch(batch)) {
    return false;
  }

  // Build into a local candidate so `out` is replaced only on whole-message
  // success. Any field-level write failure leaves `out` untouched.
  Frame candidate{};
  candidate.type = 1;  // diagnostic probe request envelope

  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  if (!writer.u32(batch.first_tick)) {
    return false;
  }
  if (!writer.u16(static_cast<uint16_t>(batch.count))) {
    return false;
  }
  for (size_t i = 0; i < batch.count; ++i) {
    if (!writer.u8(batch.masks[i])) {
      return false;
    }
  }

  candidate.size = writer.position();
  out = candidate;
  return true;
}

inline bool decode_input_payload(const Frame& frame, InputBatch& out) noexcept {
  // This function never inspects frame.type: routing by type is the caller's
  // responsibility.

  // Reject an impossible declared size before constructing a reader, so the
  // reader can never see more bytes than the payload array actually owns.
  if (frame.size > frame.payload.size()) {
    return false;
  }

  ByteReader reader(frame.payload.data(), frame.size);

  uint32_t first_tick = 0;
  if (!reader.u32(first_tick)) {
    return false;
  }

  uint16_t count16 = 0;
  if (!reader.u16(count16)) {
    return false;
  }

  // Narrowing is not range checking: validate the uint16 explicitly against the
  // batch limit before using it as a count.
  if (count16 < 1 || count16 > kMaxBatchInputs) {
    return false;
  }

  const size_t count = static_cast<size_t>(count16);

  // Exact payload size policy: precisely `count` mask bytes must remain, so
  // both truncated and over-long payloads are rejected.
  if (reader.remaining() != count) {
    return false;
  }

  // Fill a local candidate and validate it before publishing the message.
  // A malformed late mask must not partially update the caller's output.
  InputBatch candidate{};
  candidate.first_tick = first_tick;
  candidate.count = count;
  for (size_t i = 0; i < count; ++i) {
    if (!reader.u8(candidate.masks[i])) {
      return false;
    }
  }

  if (!reader.at_end()) {
    return false;
  }

  if (!valid_input_batch(candidate)) {
    return false;
  }

  out = candidate;
  return true;
}

}  // namespace study_net

#endif  // STUDY_NET_INPUT_CODEC_H
