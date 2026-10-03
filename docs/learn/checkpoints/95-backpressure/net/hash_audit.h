#ifndef STUDY_NET_HASH_AUDIT_H
#define STUDY_NET_HASH_AUDIT_H

// Bounded, single-owner audit queue for periodic StateStamp exchanges.
//
// Purpose: collect the local simulation's periodic snapshots and the snapshots
// received from the remote endpoint, then walk them in tick order to produce
// per-tick agreement results. The queue is deliberately small
// (kHashCapacity slots spanning kHashWindow steps) so a peer cannot make the
// auditor retain unbounded future state.
//
// Origin vs player role
// ---------------------
// A StateStamp always carries CANONICAL player-role fields: `host_hash` is the
// hash attributed to the host role and `peer_hash` the hash attributed to the
// peer role, whichever endpoint produced the stamp. The local_ and remote_
// queues record ORIGIN, not role: local_ holds stamps produced by this
// simulation and remote_ holds stamps received from the peer. Consequently a
// comparison at one tick asks whether the two origins agree about each role:
//   host agreement : local_[0].host_hash == remote_[0].host_hash
//   peer agreement : local_[0].peer_hash == remote_[0].peer_hash
// Changing the observer does not exchange the canonical player fields.
//
// Ordering and ownership
// ----------------------
// Every method is called from a single owner, serialized with the rest of the
// simulation step. There are no mutexes, no I/O, and no internally managed threads.
// Missing slots are represented by std::optional, so a hash value of zero is
// never overloaded as "absent" (there is no auto-zeroing).
//
// The queue must receive every periodic snapshot, including tick 0, or poll()
// will stall on the missing origin. Callers reset a round by constructing a
// fresh HashAudit.
//
// Frontier stability
// ------------------
// next_ (the next expected tick) is advanced ONLY by poll(). Recording a stamp
// can never move it, so adversarial future input cannot drag the frontier
// forward past snapshots the caller has not yet compared.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/hash_protocol.h"

namespace study_net {

// Result of offering one stamp to the audit queue.
enum class AuditPut {
  stored,     // accepted into an empty slot
  duplicate,  // an identical stamp already occupies the slot
  conflict,   // a different stamp already occupies the slot; it is preserved
  stale,      // tick is behind next_
  too_far,    // tick is ahead but beyond kHashWindow
  invalid,    // stamp failed valid_stamp()
  exhausted   // next_ has moved past kMaxStateTick
};

// One resolved per-tick comparison. matched() reports exact agreement on both
// canonical role hashes. A mismatch is data, not an error: the caller decides
// whether to stop, report, or resynchronize.
struct Comparison {
  std::uint64_t tick = 0;
  bool host_equal = false;
  bool peer_equal = false;

  bool matched() const noexcept { return host_equal && peer_equal; }
};

class HashAudit {
 public:
  HashAudit() noexcept = default;

  // Next tick that poll() will resolve. Advances by kHashPeriod per poll.
  std::uint64_t next_tick() const noexcept { return next_; }

  // Offer a snapshot produced by this simulation.
  AuditPut record_local(const StateStamp& stamp) noexcept {
    return put(local_, stamp);
  }

  // Offer a snapshot received from the remote endpoint.
  AuditPut record_remote(const StateStamp& stamp) noexcept {
    return put(remote_, stamp);
  }

  // Resolve the earliest tick. Returns false, leaving `out` and all state
  // untouched, when either origin is still missing that tick (pending). On
  // success `out` receives the comparison, both queues shift left by one slot,
  // the trailing slot is cleared, and next_ advances by kHashPeriod. A mismatch
  // is still consumed: poll() reports data, not agreement.
  bool poll(Comparison& out) noexcept {
    if (!local_[0].has_value() || !remote_[0].has_value()) {
      return false;
    }

    Comparison candidate{};
    candidate.tick = next_;
    candidate.host_equal = local_[0]->host_hash == remote_[0]->host_hash;
    candidate.peer_equal = local_[0]->peer_hash == remote_[0]->peer_hash;

    for (std::size_t i = 0; i + 1 < kHashCapacity; ++i) {
      local_[i] = local_[i + 1];
      remote_[i] = remote_[i + 1];
    }
    local_[kHashCapacity - 1].reset();
    remote_[kHashCapacity - 1].reset();

    next_ += kHashPeriod;
    out = candidate;
    return true;
  }

 private:
  using Slot = std::optional<StateStamp>;
  using Queue = std::array<Slot, kHashCapacity>;

  // Shared acceptance path for both origins. The whole stamp is validated
  // first; frontier conditions are checked from nearest to farthest; a slot
  // already holding a differing stamp is left in place (conflict preserving).
  AuditPut put(Queue& queue, const StateStamp& stamp) noexcept {
    if (!valid_stamp(stamp)) {
      return AuditPut::invalid;
    }
    if (next_ > kMaxStateTick) {
      return AuditPut::exhausted;
    }
    if (stamp.tick < next_) {
      return AuditPut::stale;
    }

    const std::uint64_t offset = stamp.tick - next_;
    if (offset >= kHashWindow) {
      return AuditPut::too_far;
    }

    const std::size_t index = static_cast<std::size_t>(offset / kHashPeriod);
    Slot& slot = queue[index];
    if (slot.has_value()) {
      return same_stamp(*slot, stamp) ? AuditPut::duplicate : AuditPut::conflict;
    }
    slot = stamp;
    return AuditPut::stored;
  }

  Queue local_{};
  Queue remote_{};
  std::uint64_t next_ = 0;
};

}  // namespace study_net

#endif  // STUDY_NET_HASH_AUDIT_H
