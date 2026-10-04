#ifndef STUDY_NET_RESULT_HANDOFF_H_
#define STUDY_NET_RESULT_HANDOFF_H_

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

#include "match_submission.h"

namespace study_net {

// Single-owner handoff of a MatchRecord claim and its Reply completion.
//
// This type has NO internal synchronization. The owner must serialize every
// call through an external mutex or a single owner event loop. In particular:
//  - A successful local claim is NOT distributed exactly-once; it only means
//    this owner won the local transition out of Stage::open. Other owners,
//    retries, or the remote peer can still act independently.
//  - request() returns a copy of the stored snapshot. Copying it does not
//    synchronize or freeze the simulation. Callers must perform claim() and
//    take the simulation snapshot
//    within the same ownership boundary (e.g. the same locked section) so the
//    two describe the same instant.
class ResultHandoff {
 public:
  enum class Stage { open, inflight, confirmed, unconfirmed, not_eligible };

  explicit ResultHandoff(uint64_t key) : key_(key) {
    if (key_ == 0) throw std::invalid_argument("ResultHandoff key must be nonzero");
  }

  ResultHandoff(const ResultHandoff&) = delete;
  ResultHandoff& operator=(const ResultHandoff&) = delete;

  Stage stage() const noexcept { return stage_; }
  uint64_t key() const noexcept { return key_; }

  // One local claim. Returns false once this handoff has left open. With a
  // record, requires record->key == key() before any mutation and throws
  // std::invalid_argument otherwise; on success it stores an immutable request
  // snapshot and moves to inflight. Without a record it moves to not_eligible.
  bool claim(std::optional<MatchRecord> record) {
    if (stage_ != Stage::open) return false;
    if (!record) {
      stage_ = Stage::not_eligible;
      return true;
    }
    if (record->key != key_) {
      throw std::invalid_argument("ResultHandoff claim key mismatch");
    }
    request_ = std::move(record);
    stage_ = Stage::inflight;
    return true;
  }

  // Copy of the immutable request snapshot; present only after a claim with a
  // record. Copying does not synchronize other state (see class comment).
  std::optional<MatchRecord> request() const { return request_; }

  // Accepts a completion for this handoff. Returns false unless the stage is
  // inflight and key matches the constructor key. A confirmed Reply is accepted
  // as confirmed only when its receipt key/row and player IDs match the claimed
  // request; every other completion (unconfirmed, stopped, unknown, or a bad
  // receipt) is accepted as unconfirmed. Returns true for any accepted
  // completion so the owner can retire the entry. It does not claim the remote
  // operation failed to commit, and never resets to open or retries.
  bool complete(uint64_t key, const Reply& reply) {
    if (stage_ != Stage::inflight || key != key_) return false;

    bool confirmed = false;
    if (reply.kind == Reply::confirmed && request_.has_value()) {
      const Receipt& r = reply.receipt;
      confirmed = r.key == key_ && r.row > 0 &&
                  r.player_a == request_->player_a &&
                  r.player_b == request_->player_b;
    }
    stage_ = confirmed ? Stage::confirmed : Stage::unconfirmed;
    return true;
  }

 private:
  const uint64_t key_;
  Stage stage_{Stage::open};
  std::optional<MatchRecord> request_;
};

}  // namespace study_net

#endif  // STUDY_NET_RESULT_HANDOFF_H_
