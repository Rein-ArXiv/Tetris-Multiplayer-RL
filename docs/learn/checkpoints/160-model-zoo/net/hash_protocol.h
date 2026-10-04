#ifndef STUDY_NET_HASH_PROTOCOL_H
#define STUDY_NET_HASH_PROTOCOL_H

// Periodic simulation-state hash exchange for the bounded teaching study.
//
// A StateStamp carries three little-endian u64 fields on the wire (type 21,
// exactly 24 payload bytes): the completed-step counter `tick` and two
// canonical state hashes, `host_hash` and `peer_hash`.
//
// `tick` counts COMPLETED simulation steps and starts at 0. The simulation is
// driven by u32-indexed input records, so the largest meaningful tick is
// UINT32_MAX + 1: the step count after the final u32 input. Anything larger is
// not representable by the run and is rejected.
//
// Hashes are opaque: every bit pattern, including all-zero, is a valid hash.
// Only the tick participates in validation, via valid_stamp().
//
// Both hash fields are CANONICAL player-role values, not local/remote origin
// flags. host_hash is the state hash attributed to the host role and
// peer_hash the state hash attributed to the peer role, regardless of which
// endpoint computed the stamp. The audit layer records stamps received from
// the local simulation and from the remote endpoint separately, but the
// player role lives in the field, never in the queue.

#include <cstddef>
#include <cstdint>

#include "net/byte_codec.h"
#include "net/framing.h"

namespace study_net {

// Application frame type for a state stamp.
inline constexpr std::uint8_t kStateStampType = 21;

// Exact encoded payload size: three little-endian u64 fields.
inline constexpr std::size_t kStateStampBytes = 24;

// Largest valid completed-step counter: the count after the final u32 input.
inline constexpr std::uint64_t kMaxStateTick =
    static_cast<std::uint64_t>(UINT32_MAX) + 1;

// Periodic exchange cadence, in steps, and the bounded audit window.
inline constexpr std::uint64_t kHashPeriod = 4;
inline constexpr std::size_t kHashCapacity = 8;

// Number of future steps the bounded audit can retain: 4 * 8 = 32.
inline constexpr std::uint64_t kHashWindow =
    static_cast<std::uint64_t>(kHashPeriod) *
    static_cast<std::uint64_t>(kHashCapacity);

static_assert(kStateStampType <= 255u, "frame type must fit in a u8");
static_assert(kHashPeriod > 0, "hash period must be non-zero");
static_assert(kHashCapacity > 0, "hash capacity must be non-zero");
static_assert(kMaxStateTick % kHashPeriod == 0,
              "the maximum tick must land on the periodic grid");
static_assert(kStateStampBytes <= kMaxPayloadBytes,
              "a state stamp must fit in one frame payload");

// One periodic snapshot. `tick` is a completed-step count; the two hashes are
// canonical per-role values. A default stamp is the tick-0 snapshot with both
// hashes zero, which is a legal snapshot because all hash bits are valid.
struct StateStamp {
  std::uint64_t tick = 0;
  std::uint64_t host_hash = 0;
  std::uint64_t peer_hash = 0;
};

// A stamp is valid only if it lands on the periodic grid and is representable
// by a run driven by u32 inputs. Hash contents are deliberately unrestricted.
inline bool valid_stamp(const StateStamp& stamp) noexcept {
  return stamp.tick <= kMaxStateTick && (stamp.tick % kHashPeriod) == 0;
}

// Compare every field. No tolerance: hashes are exact.
inline bool same_stamp(const StateStamp& a, const StateStamp& b) noexcept {
  return a.tick == b.tick && a.host_hash == b.host_hash &&
         a.peer_hash == b.peer_hash;
}

// Encode `stamp` into `out`. On any failure `out` is untouched: the frame is
// assembled in a local candidate and assigned only after every write succeeds.
// All validation happens before a single byte is produced.
inline bool encode_stamp(const StateStamp& stamp, Frame& out) noexcept {
  if (!valid_stamp(stamp)) {
    return false;
  }

  Frame candidate{};
  candidate.type = kStateStampType;
  candidate.size = kStateStampBytes;

  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  if (!writer.u64(stamp.tick) || !writer.u64(stamp.host_hash) ||
      !writer.u64(stamp.peer_hash)) {
    return false;
  }
  if (writer.position() != kStateStampBytes) {
    return false;
  }

  out = candidate;
  return true;
}

// Decode `frame` into `out`. The frame must be exactly a type-21 stamp, and the
// reader must consume the whole payload with no trailing bytes. The decoded
// candidate must pass valid_stamp() before `out` is assigned.
inline bool decode_stamp(const Frame& frame, StateStamp& out) noexcept {
  if (frame.type != kStateStampType) {
    return false;
  }
  if (frame.size != kStateStampBytes) {
    return false;
  }

  ByteReader reader(frame.payload.data(), frame.size);
  StateStamp candidate{};
  if (!reader.u64(candidate.tick) || !reader.u64(candidate.host_hash) ||
      !reader.u64(candidate.peer_hash)) {
    return false;
  }
  if (!reader.at_end()) {
    return false;
  }
  if (!valid_stamp(candidate)) {
    return false;
  }

  out = candidate;
  return true;
}

}  // namespace study_net

#endif  // STUDY_NET_HASH_PROTOCOL_H
