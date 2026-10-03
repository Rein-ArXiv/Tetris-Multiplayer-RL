#pragma once

// Room membership for a single two-player game room.
//
// Scope: membership bookkeeping only. No sockets, timers, READY flags, or game
// state. Peers are opaque: the room stores shared_ptr<Peer> but never inspects
// their internals and never performs I/O.
//
// What this header teaches
//   * Two membership slots (host + guest) owned by one mutex-protected object.
//   * Stale-leave prevention: a leave carries the exact ticket handed out at
//     join time, so a leave that races a re-join cannot evict the newer member.
//   * Versioned notices: every membership change bumps `revision`; a notice is
//     deliverable only while its revision is current, which stops an old
//     "peer left / room full" message from following a newer join.
//   * Removal vs resource destruction: leave/close return owning shared_ptrs
//     so cleanup can happen outside the state lock. A failed join can release
//     its argument after its locks are gone. Other aliases may retain a Peer.
//
// Lifetime contract: close stops membership and extracts slots; it does NOT
// wait for active callers or notice sends. Keep this object alive until all API
// users have finished. Pending notices can retain peers after slot removal.
// shared_ptr protects lifetime, not Peer internals. Serialize mutable endpoint
// I/O/close separately, or use a transport with a suitable concurrent contract.

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace study_net {

// Internal server-issued instance id, not an authentication credential. Every
// removal rechecks the live slot; the fields themselves prove no authority.
struct RoomMemberTicket {
    std::uint64_t epoch  = 0;  // registration epoch, unique across room lifetimes
    std::size_t   side   = 0;  // 0 = host, 1 = guest
    std::uint64_t member = 0;  // 1 = host, then 2,3,... and never reused
};

inline bool operator==(const RoomMemberTicket& a, const RoomMemberTicket& b) {
    return a.epoch == b.epoch && a.side == b.side && a.member == b.member;
}

inline bool operator!=(const RoomMemberTicket& a, const RoomMemberTicket& b) {
    return !(a == b);
}

enum class RoomJoinStatus { joined, closed, invalid_peer, no_host, full, exhausted };
enum class RoomLeaveStatus { removed, stale, exhausted };

template <class Peer>
class RoomMembership {
private:
    struct Slot {
        RoomMemberTicket      ticket{};
        std::shared_ptr<Peer> endpoint{};
    };

    static constexpr std::size_t   kHost         = 0;
    static constexpr std::size_t   kGuest        = 1;
    static constexpr std::size_t   kSlotCount    = 2;
    static constexpr std::uint64_t kHostMemberId = 1;
    static constexpr std::uint64_t kMaxMember =
        std::numeric_limits<std::uint64_t>::max();
    static constexpr std::uint64_t kMaxRevision =
        std::numeric_limits<std::uint64_t>::max();

public:
    // A notification addressed to one live slot. `revision` and `present` are
    // re-checked before entering the send callback. A leave can change state
    // during that callback, so delivery orders notices, not instantaneous truth.
    struct Notice {
        std::uint64_t         revision = 0;
        RoomMemberTicket      recipient{};
        std::shared_ptr<Peer> endpoint{};
        std::size_t           present = 0;
    };

    struct JoinResult {
        RoomJoinStatus                  status = RoomJoinStatus::closed;
        std::optional<RoomMemberTicket> ticket{};
    };

    struct LeaveResult {
        RoomLeaveStatus       status = RoomLeaveStatus::stale;
        std::shared_ptr<Peer> departed{};
        std::optional<Notice> notice{};
        bool                  empty = false;
    };

    // `revision` and `next_member` are test-injection points:
    //   * revision    - starts at 1; start at max to exercise exhaustion.
    //   * next_member - next guest member id (default 2); 0 means the id space
    //                   is already exhausted, and 1 is rejected because member
    //                   id 1 is reserved for the host.
    // Production callers use the defaults.
    explicit RoomMembership(std::uint64_t epoch,
                            std::shared_ptr<Peer> host,
                            std::uint64_t revision = 1,
                            std::uint64_t next_member = 2)
        : epoch_(epoch), next_member_(next_member), revision_(revision) {
        if (epoch_ == 0) {
            throw std::invalid_argument("RoomMembership: epoch must be non-zero");
        }
        if (!host) {
            throw std::invalid_argument("RoomMembership: host must not be null");
        }
        if (revision_ == 0) {
            throw std::invalid_argument("RoomMembership: revision must be non-zero");
        }
        if (next_member_ == 1) {
            throw std::invalid_argument(
                "RoomMembership: member id 1 is reserved for the host");
        }
        slots_[kHost].ticket = RoomMemberTicket{epoch_, kHost, kHostMemberId};
        slots_[kHost].endpoint = std::move(host);
        // slots_[kGuest] stays empty.
    }

    RoomMembership(const RoomMembership&) = delete;
    RoomMembership& operator=(const RoomMembership&) = delete;
    RoomMembership(RoomMembership&&) = delete;
    RoomMembership& operator=(RoomMembership&&) = delete;

    // Snapshot of the notice currently addressed to `side`, or nullopt when the
    // room is closed, the side is invalid, or the slot is empty. Callers take
    // these after a join or a leave to learn which peers to notify; the snapshot
    // reflects the current revision, which is not necessarily the revision
    // produced by the caller's own join.
    std::optional<Notice> snapshot_to(std::size_t side) const {
        std::lock_guard<std::mutex> lock(state_);
        if (closed_ || side >= kSlotCount || !slots_[side].endpoint) {
            return std::nullopt;
        }
        return Notice{revision_, slots_[side].ticket, slots_[side].endpoint,
                      present_locked()};
    }

    // Admit the guest. Lock order is always send_gate_ then state_, never the
    // reverse. Holding the gate across the commit stops a new membership from
    // becoming visible in the middle of a notice send that was already
    // validated. This function performs no send itself.
    //
    // Failure does not change membership. Passing std::move to this by-value
    // parameter can still consume the caller's shared_ptr. Because function
    // parameters are destroyed after local locks, its shared_ptr is released
    // only after send_gate_ and state_ are unlocked.
    JoinResult join_guest(std::shared_ptr<Peer> peer) {
        JoinResult result;

        std::lock_guard<std::mutex>  send_lock(send_gate_);
        std::unique_lock<std::mutex> state_lock(state_);

        if (closed_) {
            result.status = RoomJoinStatus::closed;
            return result;
        }
        if (!peer || peer == slots_[kHost].endpoint) {
            result.status = RoomJoinStatus::invalid_peer;
            return result;
        }
        if (!slots_[kHost].endpoint) {
            result.status = RoomJoinStatus::no_host;
            return result;
        }
        if (slots_[kGuest].endpoint) {
            result.status = RoomJoinStatus::full;
            return result;
        }
        if (revision_ == kMaxRevision || next_member_ == 0) {
            result.status = RoomJoinStatus::exhausted;
            return result;
        }

        // Commit point. Everything above left the room untouched; from here the
        // guest slot is occupied and the revision advances, so any notice built
        // before this join becomes stale.
        ++revision_;
        const std::uint64_t member = next_member_;
        next_member_ = (member == kMaxMember) ? 0 : member + 1;

        slots_[kGuest].ticket = RoomMemberTicket{epoch_, kGuest, member};
        slots_[kGuest].endpoint = std::move(peer);

        result.status = RoomJoinStatus::joined;
        result.ticket = slots_[kGuest].ticket;
        return result;
    }

    // Remove the holder of `ticket`. Only the state lock is taken because a
    // leave never calls out. A racing leave with the same ticket finds the slot
    // already reset and reports `stale`, so exactly one wins.
    LeaveResult leave(const RoomMemberTicket& ticket) {
        LeaveResult result;

        std::unique_lock<std::mutex> state_lock(state_);

        if (closed_ || ticket.side >= kSlotCount) {
            result.status = RoomLeaveStatus::stale;
            return result;
        }
        Slot& slot = slots_[ticket.side];
        if (!slot.endpoint || slot.ticket.epoch != ticket.epoch ||
            slot.ticket.member != ticket.member) {
            result.status = RoomLeaveStatus::stale;
            return result;
        }
        if (revision_ == kMaxRevision) {
            // Refuse this mutation: we cannot mint a fresher revision. Wrapping would
            // let stale notices match again, so the caller must close().
            result.status = RoomLeaveStatus::exhausted;
            return result;
        }

        // Commit point. The removed resource moves to result.departed and stays
        // owned by the caller; it is never destroyed here or under the lock.
        ++revision_;
        result.status = RoomLeaveStatus::removed;
        result.departed = std::move(slot.endpoint);
        slot.ticket = RoomMemberTicket{};
        slot.endpoint.reset();

        const std::size_t remaining = present_locked();
        if (remaining > 0) {
            const std::size_t other = (ticket.side == kHost) ? kGuest : kHost;
            result.notice = Notice{revision_, slots_[other].ticket,
                                   slots_[other].endpoint, remaining};
        } else {
            closed_ = true;
            result.empty = true;
        }
        return result;
    }

    // Mark the room closed and hand every remaining endpoint back to the
    // caller. Idempotent: the second call returns two null pointers. The room
    // does not wait for senders. Slots are empty, but notices/callers may still
    // own references or be inside a previously validated callback.
    std::array<std::shared_ptr<Peer>, kSlotCount> close() {
        std::array<std::shared_ptr<Peer>, kSlotCount> drained{};

        std::lock_guard<std::mutex> state_lock(state_);
        if (closed_) {
            return drained;
        }
        closed_ = true;  // invalidates every outstanding notice; no bump needed
        for (std::size_t side = 0; side < kSlotCount; ++side) {
            drained[side] = std::move(slots_[side].endpoint);
            slots_[side].ticket = RoomMemberTicket{};
            slots_[side].endpoint.reset();
        }
        return drained;
    }

    bool closed() const {
        std::lock_guard<std::mutex> lock(state_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(state_);
        return present_locked();
    }

    std::optional<RoomMemberTicket> host_ticket() const {
        std::lock_guard<std::mutex> lock(state_);
        if (closed_ || !slots_[kHost].endpoint) {
            return std::nullopt;
        }
        return slots_[kHost].ticket;
    }

    // Deliver a notice if it still matches live state. The sender runs while
    // the send gate is held and the state lock is released, so a leave may
    // advance the revision during the callback. The promise is only that a
    // newer notice cannot overlap or pass an older send, not that the notice is
    // instantaneous.
    //
    // The state lock is released before the callback and the gate is kept, so
    // the sender must return within a bounded time and must not re-enter this
    // room (that would deadlock on the gate). The endpoint shared_ptr keeps the
    // peer alive for the duration of the call. A false return leaves the room
    // unmodified; the caller decides on cleanup.
    template <class Sender>
    bool deliver(const Notice& notice, Sender&& sender) {
        static_assert(
            std::is_nothrow_invocable_r_v<bool, Sender&, const std::shared_ptr<Peer>&,
                                  std::size_t>,
            "Sender must be noexcept bool(const shared_ptr<Peer>&, size_t)");

        std::unique_lock<std::mutex> send_lock(send_gate_);
        {
            std::lock_guard<std::mutex> state_lock(state_);
            if (closed_ || notice.revision != revision_ ||
                notice.recipient.side >= kSlotCount) {
                return false;
            }
            const Slot& slot = slots_[notice.recipient.side];
            // A Notice is publicly constructible, so trust it only when every
            // field agrees with the live slot. This is an internal contract
            // with a trusted caller, not validation of untrusted input.
            if (!slot.endpoint || !(slot.ticket == notice.recipient) ||
                slot.endpoint != notice.endpoint ||
                notice.present != present_locked()) {
                return false;
            }
        }

        // State lock released, gate still held: no other deliver() can
        // interleave, and a join blocks on the gate. Leave/close can still run.
        return sender(notice.endpoint, notice.present);
    }

private:
    std::size_t present_locked() const {
        std::size_t count = 0;
        for (const Slot& slot : slots_) {
            if (slot.endpoint) {
                ++count;
            }
        }
        return count;
    }

    std::uint64_t epoch_       = 0;
    std::uint64_t next_member_ = 2;
    std::uint64_t revision_    = 1;
    bool          closed_      = false;
    std::array<Slot, kSlotCount> slots_{};

    mutable std::mutex state_;      // guards slots_, revision_, closed_, ...
    mutable std::mutex send_gate_;  // serializes deliver() callbacks
};

}  // namespace study_net
