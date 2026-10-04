#ifndef STUDY_NET_ACCEPTANCE_LOBBY_H
#define STUDY_NET_ACCEPTANCE_LOBBY_H

// Acceptance handshake for the bounded teaching study protocol.
//
// This is deliberately small and single-owner:
//   * No sockets, no clocks, no threads, no mutexes. The caller supplies the
//     monotonic time and serializes every call.
//   * The object never creates or closes a socket. It only records handshake
//     state and owns the byte-level parsers.
//   * The type is not thread safe.
//
// Wire messages (application types validated here, strictly):
//   READY  type 17: payload exactly 1 byte, value 1 accept or 0 decline.
//   CANCEL type 11: payload exactly 0 bytes.
// Anything else, or a wrong-sized READY/CANCEL, is a protocol_error. This
// strictness is a teaching choice; a tolerant root protocol may instead keep
// waiting on unknown types. No auth or match-found framing is added here.
//
// Consumption invariant: once a side is marked ready we never parse that side
// again. Its READY(1) frame is consumed exactly once; every byte after
// it (game input, a partial next header, anything) stays in that side parser as
// an opaque tail and travels with the handoff. The caller therefore transfers
// both the socket and parser: the kernel can no longer return the bytes
// this lobby already consumed.
//
// Each FrameParser has a fixed kReceiveCapacity (64-byte) buffer. Overflow
// aborts this phase. Socket/kernel buffers and later game state are separate.

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "net/framing.h"

namespace study_net {

inline constexpr std::uint8_t kLobbyReadyType = 17;
inline constexpr std::uint8_t kLobbyCancelType = 11;

inline constexpr std::size_t kLobbySides = 2;

// poll/feed/eof change only waiting; invalid side arguments are rejected first.
// accepted still permits close (announcement failure) before take. `clock_error`
// and `invalid_side` are transient argument errors. They surface in a
// LobbyStep but are never stored in the object.
enum class LobbyState {
  waiting,
  accepted,
  handed_off,
  declined,
  peer_closed,
  timed_out,
  protocol_error,
  buffer_limit,
  invalid_config,
  clock_error,
  invalid_side,
};

// Outbound action the caller must deliver. `ready` locally confirms an accept;
// `decline` tells the peer the handshake failed.
enum class LobbyNotice {
  none,
  ready,
  decline,
};

struct LobbyStep {
  LobbyState state{LobbyState::waiting};
  LobbyNotice notice{LobbyNotice::none};
  // Index of the peer the notice is addressed to when notice != none.
  std::size_t peer{0};
};

// Ownership transfer produced by AcceptanceLobby::take. Move-only and default
// constructible so a caller can hold it while driving the accepted sockets.
// `parsers[i]` carries side i plus any unconsumed tail after its READY frame.
struct LobbyHandoff {
  std::array<FrameParser, kLobbySides> parsers{};

  LobbyHandoff() noexcept = default;
  LobbyHandoff(const LobbyHandoff&) = delete;
  LobbyHandoff& operator=(const LobbyHandoff&) = delete;
  LobbyHandoff(LobbyHandoff&& other) noexcept : parsers(std::move(other.parsers)) {
    other.parsers = {};
  }
  LobbyHandoff& operator=(LobbyHandoff&& other) noexcept {
    if (this != &other) {
      parsers = std::move(other.parsers);
      other.parsers = {};
    }
    return *this;
  }
};

class AcceptanceLobby {
 public:
  // `start` and `timeout` are opaque caller-clock ticks; timeout has no magic
  // unit here. A zero timeout is a configuration error and settles the object
  // at invalid_config, from which it can never become waiting.
  explicit AcceptanceLobby(std::uint64_t start, std::uint64_t timeout) noexcept
      : last_(start), start_(start), timeout_(timeout) {
    if (timeout_ == 0) {
      state_ = LobbyState::invalid_config;
    }
  }

  // The previous phase may have already read part or all of READY.
  // Transfer its parsers together with the socket pair; clear the source.
  AcceptanceLobby(std::uint64_t start, std::uint64_t timeout,
                  LobbyHandoff&& prefix) noexcept : AcceptanceLobby(start, timeout) {
    parsers_ = std::move(prefix.parsers);
    prefix.parsers = {};
  }

  AcceptanceLobby(const AcceptanceLobby&) = delete;
  AcceptanceLobby& operator=(const AcceptanceLobby&) = delete;
  AcceptanceLobby(AcceptanceLobby&&) = delete;
  AcceptanceLobby& operator=(AcceptanceLobby&&) = delete;

  // Deadline check. Settled states are returned unchanged, even when `now` went
  // backwards, so a finished lobby never turns into a clock error.
  // While waiting, `now < last_` is a transient clock_error with no mutation.
  // The deadline is measured from the original start and is never extended by
  // activity: updating last_ only tracks monotonicity.
  LobbyStep poll(std::uint64_t now) noexcept {
    if (state_ != LobbyState::waiting) {
      return current();
    }
    if (now < last_) {
      return LobbyStep{LobbyState::clock_error};
    }
    last_ = now;
    // now >= last_ >= start_, so this subtraction cannot underflow.
    if (now - start_ >= timeout_) {
      state_ = LobbyState::timed_out;
    }
    return current();
  }

  // Feed bytes for `side`. The caller guarantees `data` is valid when count>0.
  //
  // The side check runs before any time or parser work, so an out-of-range side
  // is always a pure invalid_side report. Then poll runs; a settled or timed
  // out lobby (or a clock error) short-circuits and this call appends nothing.
  //
  // Appending happens even after the side is ready, so late bytes are kept as
  // the opaque tail instead of being dropped. Appending more than the fixed
  // buffer holds is buffer_limit.
  //
  // Before the side is ready, only ONE frame is consumed. Extra frames in the
  // same read stay in the parser for the game phase.
  LobbyStep feed(std::size_t side, const std::uint8_t* data, std::size_t count,
                 std::uint64_t now) noexcept {
    if (side >= kLobbySides) {
      return LobbyStep{LobbyState::invalid_side};
    }
    const LobbyStep polled = poll(now);
    if (polled.state != LobbyState::waiting) {
      return polled;
    }
    if (!parsers_[side].append(data, count)) {
      state_ = LobbyState::buffer_limit;
      return current();
    }
    if (ready_[side]) {
      return current();
    }
    Frame frame{};
    const ParseStatus status = parsers_[side].next(frame);
    if (status == ParseStatus::need_more) {
      return current();
    }
    if (status == ParseStatus::error) {
      state_ = LobbyState::protocol_error;
      return current();
    }
    if (frame.type == kLobbyCancelType) {
      if (frame.size != 0) {
        state_ = LobbyState::protocol_error;
        return current();
      }
      return decline(side);
    }
    if (frame.type != kLobbyReadyType) {
      state_ = LobbyState::protocol_error;
      return current();
    }
    if (frame.size != 1) {
      state_ = LobbyState::protocol_error;
      return current();
    }
    if (frame.payload[0] == 0) {
      return decline(side);
    }
    if (frame.payload[0] != 1) {
      state_ = LobbyState::protocol_error;
      return current();
    }
    ready_[side] = true;
    if (ready_[0] && ready_[1]) {
      state_ = LobbyState::accepted;
    }
    return current(LobbyNotice::ready, 1 - side);
  }

  // EOF is only meaningful while waiting. If one side is ready but the peer
  // never will be, the handshake can no longer complete, so the lobby aborts as
  // peer_closed. Once both sides are ready the commit (accepted) is settled
  // here: a later peer EOF belongs to the next phase and is ignored.
  LobbyStep eof(std::size_t side, std::uint64_t now) noexcept {
    if (side >= kLobbySides) {
      return LobbyStep{LobbyState::invalid_side};
    }
    const LobbyStep polled = poll(now);
    if (polled.state != LobbyState::waiting) {
      return polled;
    }
    state_ = LobbyState::peer_closed;
    return current();
  }

  // Hand the ready parsers to the caller. Only an accepted lobby can be taken.
  // On success the two parsers move out first, then the lobby is reset to two
  // fresh parsers and handed_off. On any other state `out` is left untouched.
  // A successful call replaces out; pass a fresh/fully consumed destination.
  bool take(LobbyHandoff& out) noexcept {
    if (state_ != LobbyState::accepted) {
      return false;
    }
    out.parsers[0] = std::move(parsers_[0]);
    out.parsers[1] = std::move(parsers_[1]);
    parsers_[0] = FrameParser{};
    parsers_[1] = FrameParser{};
    ready_[0] = false;
    ready_[1] = false;
    state_ = LobbyState::handed_off;
    return true;
  }

  // External shutdown, and the send-failure path. If the caller cannot deliver
  // a returned ready/decline notice it must call close, which aborts a waiting
  // or accepted lobby as peer_closed. It never touches handed_off or any other
  // settled state, so a completed handoff can never be undone.
  void close() noexcept {
    if (state_ == LobbyState::waiting || state_ == LobbyState::accepted) {
      state_ = LobbyState::peer_closed;
    }
  }

  LobbyState state() const noexcept { return state_; }

  // False for an out-of-range side. A ready side is never parsed again.
  bool ready(std::size_t side) const noexcept {
    return side < kLobbySides && ready_[side];
  }

  // Unconsumed bytes for a side, including the opaque tail after READY.
  // Out-of-range sides report 0 instead of touching storage.
  std::size_t buffered(std::size_t side) const noexcept {
    return side < kLobbySides ? parsers_[side].pending_bytes() : 0;
  }

 private:
  LobbyStep current(LobbyNotice notice = LobbyNotice::none,
                    std::size_t peer = 0) const noexcept {
    return LobbyStep{state_, notice, peer};
  }

  LobbyStep decline(std::size_t side) noexcept {
    state_ = LobbyState::declined;
    return current(LobbyNotice::decline, 1 - side);
  }

  std::array<FrameParser, kLobbySides> parsers_{};
  std::array<bool, kLobbySides> ready_{false, false};
  std::uint64_t last_{0};
  std::uint64_t start_{0};
  std::uint64_t timeout_{0};
  LobbyState state_{LobbyState::waiting};
};

}  // namespace study_net

#endif  // STUDY_NET_ACCEPTANCE_LOBBY_H
