#ifndef STUDY_NET_MATCH_SUBMISSION_H
#define STUDY_NET_MATCH_SUBMISSION_H

// match_submission.h
//
// A single-owner "submission port" that hands one finished match result to an
// external persistent service and interprets the service's reply.
//
// Scope (deliberately small, teaching-oriented):
//   * no sockets, JSON, database, rewards or authentication code
//   * fixed value types only; the class performs no heap allocations
//   * one object owns one pending request, so no copy/move and no mutex
//
// Trust model:
//   * MatchRecord is trusted, internally produced data. Validation here only
//     checks basic identity fields; it is NOT server-side verification.
//   * The caller must derive records from a trusted, completed simulation.
//
// Delivery model:
//   * This object does NOT promise exactly-once delivery or restart
//     durability. It may leave a request unconfirmed after a blocked or failed
//     call. The caller retries with the same immutable body/key, and the
//     persistent service must deduplicate by key.
//   * Transport failures can permit a bounded retry. Protocol errors stop
//     automatic retries; neither outcome proves that no earlier write occurred.

#include <cstdint>
#include <chrono>
#include <optional>
#include <type_traits>
#include <utility>

namespace study_net {

// ---------------------------------------------------------------------------
// MatchRecord: immutable description of one completed match.
// ---------------------------------------------------------------------------
struct MatchRecord {
  enum MatchWinner : std::uint8_t { draw = 0, a = 1, b = 2 };

  std::uint64_t key = 0;       // caller-chosen identity, stable across retries
  std::uint64_t round = 0;
  std::uint64_t player_a = 0;
  std::uint64_t player_b = 0;
  std::uint64_t ticks = 0;
  std::uint64_t score_a = 0;
  std::uint64_t score_b = 0;
  std::uint32_t lines_a = 0;
  std::uint32_t lines_b = 0;
  MatchWinner winner = draw;
};

inline bool operator==(const MatchRecord& l, const MatchRecord& r) noexcept {
  return l.key == r.key && l.round == r.round && l.player_a == r.player_a &&
         l.player_b == r.player_b && l.ticks == r.ticks &&
         l.score_a == r.score_a && l.score_b == r.score_b &&
         l.lines_a == r.lines_a && l.lines_b == r.lines_b &&
         l.winner == r.winner;
}

inline bool operator!=(const MatchRecord& l, const MatchRecord& r) noexcept {
  return !(l == r);
}

// ---------------------------------------------------------------------------
// Receipt / Reply: what the external service returns for an accepted record.
// ---------------------------------------------------------------------------
struct Receipt {
  std::uint64_t key = 0;
  std::uint64_t row = 0;       // server-assigned, must be > 0 when confirmed
  std::uint64_t player_a = 0;
  std::uint64_t player_b = 0;
};

struct Reply {
  enum ReplyKind : std::uint8_t { confirmed = 0, unconfirmed = 1, stopped = 2 };

  ReplyKind kind = unconfirmed;  // benign default: nothing known to be stored
  Receipt receipt{};
  std::chrono::milliseconds retry_after{0}; // minimum wait before another attempt
};

// ---------------------------------------------------------------------------
// MatchSubmission: the single-owner submission port.
// ---------------------------------------------------------------------------
class MatchSubmission {
 public:
  // Scoped enums so the two enumerator sets cannot collide in class scope.
  enum class SubmissionState { empty, ready, unconfirmed, confirmed, stopped };
  enum class SubmitStep { no_work, confirmed, unconfirmed, stopped, budget_exhausted };

  explicit MatchSubmission(unsigned max_attempts) noexcept
      : max_attempts_(max_attempts) {}

  // One object owns one fixed pending request: never copy or move it.
  MatchSubmission(const MatchSubmission&) = delete;
  MatchSubmission& operator=(const MatchSubmission&) = delete;
  MatchSubmission(MatchSubmission&&) = delete;
  MatchSubmission& operator=(MatchSubmission&&) = delete;
  ~MatchSubmission() = default;

  // Prepare the immutable request. Legal only once, from the empty state.
  // Returns false and leaves the object unchanged on invalid input.
  bool prepare(const MatchRecord& record) noexcept {
    if (state_ != SubmissionState::empty) return false;
    if (!is_valid(record)) return false;
    request_ = record;              // fixed-size copy into inline storage
    state_ = SubmissionState::ready;
    return true;
  }

  // Pointer to the owned request once prepared, otherwise nullptr.
  const MatchRecord* request() const noexcept {
    return request_.has_value() ? &request_.value() : nullptr;
  }

  // Pointer to the accepted receipt only once confirmed; otherwise nullptr.
  const Receipt* receipt() const noexcept {
    return state_ == SubmissionState::confirmed ? &receipt_ : nullptr;
  }

  SubmissionState state() const noexcept { return state_; }
  std::chrono::milliseconds retry_after() const noexcept { return retry_after_; }
  unsigned attempts() const noexcept { return attempts_; }
  unsigned max_attempts() const noexcept { return max_attempts_; }

  // One submission attempt. Sender must be noexcept-invocable as
  // Reply(const MatchRecord&) and must not re-enter this object or retain
  // request references after returning. The attempt budget counts calls; a
  // blocking Sender still needs its own time/cancellation policy.
  template <typename Sender>
  SubmitStep submit(Sender&& sender) {
    static_assert(
        std::is_nothrow_invocable_r_v<Reply, Sender, const MatchRecord&>,
        "Sender must be noexcept-invocable as Reply(const MatchRecord&)");

    if (state_ == SubmissionState::empty ||
        state_ == SubmissionState::confirmed ||
        state_ == SubmissionState::stopped) {
      return SubmitStep::no_work;
    }
    if (attempts_ >= max_attempts_) {
      // Budget spent: do not call. Preserve ready/unconfirmed, never map to a
      // definitive rejection.
      return SubmitStep::budget_exhausted;
    }

    ++attempts_;
    state_ = SubmissionState::unconfirmed;  // set before the callback runs
    const Reply reply = std::forward<Sender>(sender)(request_.value());

    if (reply.kind == Reply::confirmed && accepts(reply.receipt)) {
      receipt_ = reply.receipt;
      state_ = SubmissionState::confirmed;
      return SubmitStep::confirmed;
    }
    if (reply.kind == Reply::unconfirmed) {
      retry_after_ = reply.retry_after;
      return SubmitStep::unconfirmed;
    }
    // A bad receipt or stop signal ends automatic attempts, without asserting
    // that the server did not commit this operation.
    state_ = SubmissionState::stopped;
    return SubmitStep::stopped;
  }

 private:
  static bool is_valid(const MatchRecord& r) noexcept {
    if (r.key == 0 || r.round == 0) return false;
    if (r.player_a == 0 || r.player_b == 0) return false;
    if (r.player_a == r.player_b) return false;
    if (r.ticks == 0) return false;
    switch (r.winner) {
      case MatchRecord::draw:
      case MatchRecord::a:
      case MatchRecord::b:
        return true;
    }
    return false;
  }

  bool accepts(const Receipt& receipt) const noexcept {
    const MatchRecord& r = request_.value();
    return receipt.key == r.key && receipt.row > 0 &&
           receipt.player_a == r.player_a && receipt.player_b == r.player_b;
  }

  std::optional<MatchRecord> request_;  // inline storage, no allocation
  Receipt receipt_{};
  std::chrono::milliseconds retry_after_{0};
  unsigned max_attempts_ = 0;
  unsigned attempts_ = 0;
  SubmissionState state_ = SubmissionState::empty;
};

}  // namespace study_net

#endif  // STUDY_NET_MATCH_SUBMISSION_H
