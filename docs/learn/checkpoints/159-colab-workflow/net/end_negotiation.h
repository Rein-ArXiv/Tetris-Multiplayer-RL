#ifndef STUDY_NET_END_NEGOTIATION_H
#define STUDY_NET_END_NEGOTIATION_H

// End-of-game intention negotiation for the bounded study_net teaching model.
//
// One EndNegotiation object is owned by exactly one main owner and tracks a
// single round. It performs no I/O, no clock reads, no threads and no
// win/reward logic: all time values are supplied by the caller.
//
// A peer may finish its local simulation first, so a valid remote choice can
// arrive while this object is still dormant. Such a choice is staged only; it
// neither starts the timer nor records a local decision. The caller must only
// activate after its own local simulation has reached a terminal state.
//
// A stored choice is an intention, not a server verdict. The application still
// has to send an accepted local choice as a Frame and retain it before closing.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/byte_codec.h"
#include "net/framing.h"

namespace study_net {

// Wire type and encoded size for an end-choice message.
inline constexpr std::uint8_t kEndChoiceMessageType = 42;
inline constexpr std::size_t kEndChoicePayloadBytes = 9;
static_assert(kEndChoicePayloadBytes <= kMaxPayloadBytes, "end choice must fit Frame");

// The only legal intentions: restart or leave.
enum class EndChoice : std::uint8_t { restart = 1, leave = 2 };

// Exact 1/2 enum validation; every other byte pattern is rejected.
constexpr bool valid_end_choice(EndChoice choice) noexcept {
  return choice == EndChoice::restart || choice == EndChoice::leave;
}

// One end-choice message. `round` must be nonzero.
struct EndChoiceMessage {
  std::uint64_t round = 0;
  EndChoice choice = EndChoice::restart;
};

// TYPE42 payload is exactly 9 bytes: nonzero round u64 little endian, then one
// byte holding the EndChoice value. On any failure `out` is left unchanged.
inline bool encode_end_choice(const EndChoiceMessage& message,
                              Frame& out) noexcept {
  if (message.round == 0 || !valid_end_choice(message.choice)) {
    return false;
  }
  Frame candidate{};
  candidate.type = kEndChoiceMessageType;
  ByteWriter writer(candidate.payload.data(), candidate.payload.size());
  if (!writer.u64(message.round) ||
      !writer.u8(static_cast<std::uint8_t>(message.choice))) {
    return false;
  }
  candidate.size = writer.position();
  out = candidate;
  return true;
}

// Decode an exact TYPE42, 9-byte frame. On any failure `out` is unchanged.
inline bool decode_end_choice(const Frame& frame,
                              EndChoiceMessage& out) noexcept {
  if (frame.type != kEndChoiceMessageType ||
      frame.size != kEndChoicePayloadBytes) {
    return false;
  }
  ByteReader reader(frame.payload.data(), frame.size);
  std::uint64_t round = 0;
  std::uint8_t raw = 0;
  if (!reader.u64(round) || !reader.u8(raw)) {
    return false;
  }
  const EndChoice choice = static_cast<EndChoice>(raw);
  if (round == 0 || !valid_end_choice(choice)) {
    return false;
  }
  EndChoiceMessage candidate{};
  candidate.round = round;
  candidate.choice = choice;
  out = candidate;
  return true;
}

// Negotiation state. `clock_error` is reported but never stored as state.
enum class EndState {
  dormant,
  waiting,
  restart_agreed,
  leave,
  timed_out,
  transport_lost,
  clock_error
};

// Result of staging a choice.
enum class ChoicePut {
  stored,
  duplicate,
  conflict,
  inactive,
  closed,
  invalid,
  old_round,
  future_round,
  clock_error
};

// Single-round end negotiation. Same-connection round monotonicity is owned by
// a RoundGate, not by this class; a wrong-round message never starts a round.
class EndNegotiation {
 public:
  // Reject a zero round or zero timeout; a valid object starts dormant.
  static std::optional<EndNegotiation> create(std::uint64_t round,
                                              std::uint64_t timeout) noexcept {
    if (round == 0 || timeout == 0) {
      return std::nullopt;
    }
    return EndNegotiation(round, timeout);
  }

  EndNegotiation() = delete;

  // Report the state at `now`. A backwards clock reports clock_error without
  // mutating persistent state. Timeout compares elapsed time (now - start_) with timeout_.
  // activate() validated start_ and monotonic calls keep the subtraction safe;
  // no absolute deadline addition can overflow.
  EndState status(std::uint64_t now) noexcept {
    if (clock_seen_ && now < last_) {
      return EndState::clock_error;
    }
    clock_seen_ = true;
    last_ = now;
    if (state_ == EndState::waiting && now - start_ >= timeout_) {
      state_ = EndState::timed_out;
    }
    return state_;
  }

  // Start the timer once, from dormant. Any later call changes nothing.
  bool activate(std::uint64_t now) noexcept {
    if (state_ != EndState::dormant) {
      return false;
    }
    if (status(now) == EndState::clock_error) {
      return false;
    }
    start_ = now;
    state_ = EndState::waiting;
    evaluate();
    return true;
  }

  // Stage the local choice. The first stored choice is immutable.
  ChoicePut choose(EndChoice choice, std::uint64_t now) noexcept {
    if (!valid_end_choice(choice)) {
      return ChoicePut::invalid;
    }
    const EndState current = status(now);
    if (current == EndState::clock_error) {
      return ChoicePut::clock_error;
    }
    if (current == EndState::dormant) {
      return ChoicePut::inactive;
    }
    if (current != EndState::waiting) {
      return ChoicePut::closed;
    }
    if (local_.has_value()) {
      return *local_ == choice ? ChoicePut::duplicate : ChoicePut::conflict;
    }
    local_ = choice;
    evaluate();
    return ChoicePut::stored;
  }

  // Stage a remote choice. Round checks happen before any clock mutation.
  ChoicePut receive(const EndChoiceMessage& message,
                    std::uint64_t now) noexcept {
    if (message.round == 0 || !valid_end_choice(message.choice)) {
      return ChoicePut::invalid;
    }
    if (message.round < round_) {
      return ChoicePut::old_round;
    }
    if (message.round > round_) {
      return ChoicePut::future_round;
    }
    const EndState current = status(now);
    if (current == EndState::clock_error) {
      return ChoicePut::clock_error;
    }
    if (current != EndState::dormant && current != EndState::waiting) {
      return ChoicePut::closed;
    }
    if (remote_.has_value()) {
      return *remote_ == message.choice ? ChoicePut::duplicate
                                        : ChoicePut::conflict;
    }
    remote_ = message.choice;
    evaluate();
    return ChoicePut::stored;
  }

  // The connection ended. A lost transport revokes any chance to restart over
  // this connection; it awards neither a winner nor a loss.
  EndState transport_ended(std::uint64_t now) noexcept {
    const EndState current = status(now);
    if (current == EndState::clock_error) {
      return EndState::clock_error;
    }
    if (state_ == EndState::dormant || state_ == EndState::waiting ||
        state_ == EndState::restart_agreed) {
      state_ = EndState::transport_lost;
    }
    return state_;
  }

  std::uint64_t round() const noexcept { return round_; }

 private:
  EndNegotiation(std::uint64_t round, std::uint64_t timeout) noexcept
      : round_(round), timeout_(timeout) {}

  // Resolve agreement only while waiting. A remote early leave does not end a
  // dormant object or pretend our own game ended; activation then makes it
  // leave. Once the timeout has been observed first, a late choice cannot
  // revive an agreement.
  void evaluate() noexcept {
    if (state_ != EndState::waiting) {
      return;
    }
    if (local_.has_value() && *local_ == EndChoice::leave) {
      state_ = EndState::leave;
      return;
    }
    if (remote_.has_value() && *remote_ == EndChoice::leave) {
      state_ = EndState::leave;
      return;
    }
    if (local_.has_value() && remote_.has_value() &&
        *local_ == EndChoice::restart && *remote_ == EndChoice::restart) {
      state_ = EndState::restart_agreed;
    }
  }

  std::uint64_t round_ = 0;
  std::uint64_t timeout_ = 0;
  std::uint64_t start_ = 0;
  std::uint64_t last_ = 0;
  bool clock_seen_ = false;
  EndState state_ = EndState::dormant;
  std::optional<EndChoice> local_{};
  std::optional<EndChoice> remote_{};
};

}  // namespace study_net

#endif  // STUDY_NET_END_NEGOTIATION_H
