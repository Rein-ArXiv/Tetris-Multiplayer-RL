#ifndef STUDY_NET_SEED_PROTOCOL_H
#define STUDY_NET_SEED_PROTOCOL_H

// Seed handshake wire encoding for the bounded teaching study.
//
// This header is pure value/codec logic: no sockets, no threads, no dynamic
// allocation. It defines the small Config record the handshake agrees on and
// the builders/validators for the three message types:
//
//   HELLO (type 10, 6-byte payload)   u16 version, u32 rules
//   OFFER (type 11, 20-byte payload)  the host's proposed Config
//   ACK   (type 12, 20-byte payload)  the peer echoing that same Config
//
// The frame shape and its 32-byte payload cap come from net/framing.h; the
// explicit little-endian codec comes from net/byte_codec.h. A Config payload
// is 20 bytes, so it fits with room to spare.

#include <cstddef>
#include <cstdint>

#include "net/framing.h"
#include "net/byte_codec.h"

namespace study_net {

// ---------------------------------------------------------------------------
// Protocol constants
// ---------------------------------------------------------------------------

// Handshake revision and ruleset id. Both are fixed for this study and are
// checked on every received hello/config so mismatched peers are rejected.
inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint32_t kRulesetId = 1;

// Message type tags (the Frame::type byte).
inline constexpr std::uint8_t kTypeHello = 10;
inline constexpr std::uint8_t kTypeOffer = 11;
inline constexpr std::uint8_t kTypeAck = 12;

// Encoded payload sizes: HELLO = u16 + u32; Config = u16 + u32 + u64 + u32 +
// u8 + u8.
inline constexpr std::size_t kHelloPayloadBytes = 6;
inline constexpr std::size_t kConfigPayloadBytes = 20;

static_assert(kHelloPayloadBytes <= kMaxPayloadBytes,
              "hello payload must fit the frame payload cap");
static_assert(kConfigPayloadBytes <= kMaxPayloadBytes,
              "config payload must fit the frame payload cap");

// Teaching-only policy bounds. They are deliberately permissive: zeroing a
// field is always allowed, and seed 0 is a legitimate value (this layer never
// rewrites or normalizes the seed).
inline constexpr std::uint32_t kMaxCountdownTicks = 600;
inline constexpr std::uint8_t kMaxInputDelay = 30;

// The receiver_role byte is always the peer in this initial direct handshake
// (it is not the sender's role). A host's own local role of 1 is intentionally
// kept outside Config and is never placed on the wire.
inline constexpr std::uint8_t kReceiverRolePeer = 2;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

// The agreed seed parameters. Defaults are a valid configuration; brace
// initialization follows the declaration order (seed, countdown, delay).
struct Config {
  std::uint64_t seed = 0;
  std::uint32_t countdown_ticks = 120;
  std::uint8_t input_delay = 2;
};

// Field-by-field equality. Two configs match only if every field matches.
inline bool equal_config(const Config& a, const Config& b) noexcept {
  return a.seed == b.seed &&
         a.countdown_ticks == b.countdown_ticks &&
         a.input_delay == b.input_delay;
}

// Policy check only. `seed` is unconstrained (0 included); the other two
// fields must stay within the teaching bounds.
inline bool valid_config(const Config& config) noexcept {
  return config.countdown_ticks <= kMaxCountdownTicks &&
         config.input_delay <= kMaxInputDelay;
}

// ---------------------------------------------------------------------------
// HELLO
// ---------------------------------------------------------------------------

// Build the HELLO the peer sends to open the handshake. The writes cannot fail
// at this size, but each one is still checked; if a write ever failed the
// frame would keep size 0 and be rejected by valid_hello().
inline Frame make_hello() noexcept {
  Frame candidate{};
  candidate.type = kTypeHello;
  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  const bool ok = writer.u16(kProtocolVersion) && writer.u32(kRulesetId);
  if (ok) {
    candidate.size = writer.position();
  }
  return candidate;
}

// A HELLO is exactly 6 bytes carrying the expected version and ruleset.
inline bool valid_hello(const Frame& frame) noexcept {
  if (frame.type != kTypeHello || frame.size != kHelloPayloadBytes) {
    return false;
  }
  ByteReader reader(frame.payload.data(), frame.size);
  std::uint16_t version = 0;
  std::uint32_t rules = 0;
  if (!reader.u16(version)) {
    return false;
  }
  if (!reader.u32(rules)) {
    return false;
  }
  if (!reader.at_end()) {
    return false;
  }
  return version == kProtocolVersion && rules == kRulesetId;
}

// ---------------------------------------------------------------------------
// OFFER / ACK (identical Config payloads)
// ---------------------------------------------------------------------------

// Encode `config` into an OFFER or ACK frame. Only those two types are
// accepted. `out` is written only after the whole payload encoded and the
// length is exactly kConfigPayloadBytes, so a rejection leaves `out` alone.
inline bool encode_config(std::uint8_t type, const Config& config,
                          Frame& out) noexcept {
  if (type != kTypeOffer && type != kTypeAck) {
    return false;
  }
  if (!valid_config(config)) {
    return false;
  }
  Frame candidate{};
  candidate.type = type;
  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  const bool ok = writer.u16(kProtocolVersion) &&
                  writer.u32(kRulesetId) &&
                  writer.u64(config.seed) &&
                  writer.u32(config.countdown_ticks) &&
                  writer.u8(config.input_delay) &&
                  writer.u8(kReceiverRolePeer);
  if (!ok || writer.position() != kConfigPayloadBytes) {
    return false;
  }
  candidate.size = kConfigPayloadBytes;
  out = candidate;
  return true;
}

// Decode a Config frame that must carry `expected` (OFFER or ACK). Every field
// is validated: version, ruleset id, receiver_role == peer, and the policy
// bounds. `out` is left untouched on any failure.
inline bool decode_config(const Frame& frame, std::uint8_t expected,
                          Config& out) noexcept {
  if (expected != kTypeOffer && expected != kTypeAck) {
    return false;
  }
  if (frame.type != expected || frame.size != kConfigPayloadBytes) {
    return false;
  }
  ByteReader reader(frame.payload.data(), frame.size);
  std::uint16_t version = 0;
  std::uint32_t rules = 0;
  Config candidate{};
  std::uint8_t receiver_role = 0;
  if (!reader.u16(version)) {
    return false;
  }
  if (!reader.u32(rules)) {
    return false;
  }
  if (!reader.u64(candidate.seed)) {
    return false;
  }
  if (!reader.u32(candidate.countdown_ticks)) {
    return false;
  }
  if (!reader.u8(candidate.input_delay)) {
    return false;
  }
  if (!reader.u8(receiver_role)) {
    return false;
  }
  if (!reader.at_end()) {
    return false;
  }
  if (version != kProtocolVersion || rules != kRulesetId) {
    return false;
  }
  if (receiver_role != kReceiverRolePeer) {
    return false;
  }
  if (!valid_config(candidate)) {
    return false;
  }
  out = candidate;
  return true;
}

}  // namespace study_net

#endif  // STUDY_NET_SEED_PROTOCOL_H
