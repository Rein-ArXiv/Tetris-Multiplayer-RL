#pragma once
// meta/admission_tickets.h
//
// Bounded, fixed-capacity store of short-lived, single-use admission tickets.
//
// This component is storage/state only: it performs no randomness generation,
// no network I/O, no authentication and no logging. It never creates entropy
// and leaves account authentication to the caller. Ticket bytes are opaque;
// all-zero tickets are accepted because token validation is the caller's job.
//
// Threading / scope:
//   A process-local std::mutex serialises issue() and consume(). It protects a
//   single process only; it is NOT distributed and does not coordinate across
//   replicas or machines. Callers needing cross-process guarantees must add
//   their own coordination.
//
// Time:
//   The caller supplies "now" explicitly so expiry is deterministic in tests.
//   Production callers should sample Clock::now() immediately around the call.
//
// Freshness contract:
//   A removed or expired ticket frees its slot, so the same key value can be
//   issued again later. There is no permanent replay history. Therefore the
//   uniqueness and unpredictability of freshly generated tickets is a separate
//   CSPRNG contract owned by the caller, not enforced here.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace study_meta {

using Ticket = std::array<unsigned char, 16>;

struct Admission {
    std::uint64_t player;
    std::uint64_t epoch;
};

enum class IssueTicket {
    issued,    // a fresh pending ticket was stored
    invalid,   // player==0, non-positive lifetime, or time overflow
    collision, // an identical live ticket already exists
    full       // no live slot for this player and no free slot
};

template <std::size_t Capacity>
class AdmissionTickets {
    static_assert(Capacity > 0, "AdmissionTickets requires Capacity > 0");

public:
    using Clock = std::chrono::steady_clock;

    // Attempt to issue a pending ticket for `admission`.
    //
    // Expired entries are purged first; then a live duplicate ticket is rejected
    // as a collision (even if it belongs to the same player); then the player's
    // existing slot is reused, otherwise any free slot is used. On any failure
    // the store is left with its prior live entries intact.
    IssueTicket issue(const Ticket& ticket, Admission admission,
                      Clock::time_point now, Clock::duration lifetime) {
        // Reject malformed input before touching any valid entry.
        if (admission.player == 0) return IssueTicket::invalid;
        if (lifetime <= Clock::duration::zero()) return IssueTicket::invalid;
        if (now > Clock::time_point::max() - lifetime) return IssueTicket::invalid;

        std::lock_guard<std::mutex> lock(mutex_);

        // Drop entries that are already expired at `now` (expires <= now).
        for (auto& slot : slots_) {
            if (slot.has_value() && slot->expires <= now) slot.reset();
        }

        // Reject any surviving entry carrying the same opaque ticket bytes.
        for (const auto& slot : slots_) {
            if (slot.has_value() && slot->ticket == ticket) {
                return IssueTicket::collision;
            }
        }

        // Prefer the player's current slot (one pending ticket per player),
        // otherwise take the first free slot.
        std::size_t target = Capacity;
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (!slots_[i].has_value()) {
                if (target == Capacity) target = i; // first free slot
            } else if (slots_[i]->admission.player == admission.player) {
                target = i; // replacement wins over a free slot
                break;
            }
        }
        if (target == Capacity) return IssueTicket::full;

        // Commit only after every rejection check has passed.
        slots_[target] = Entry{ticket, admission, now + lifetime};
        return IssueTicket::issued;
    }

    // Redeem a ticket. The mutex spans lookup, value copy, slot clear and expiry
    // check so concurrent callers cannot redeem the same ticket twice. A ticket
    // that is expired at `now` (expires <= now) is burned and rejected.
    std::optional<Admission> consume(const Ticket& ticket, Clock::time_point now) {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& slot : slots_) {
            if (!slot.has_value() || slot->ticket != ticket) continue;

            const Admission value = slot->admission;
            const Clock::time_point expires = slot->expires;
            slot.reset(); // single use: never restore or hand out a reference
            if (expires <= now) return std::nullopt;
            return value;
        }
        return std::nullopt;
    }

private:
    struct Entry {
        Ticket ticket;
        Admission admission;
        Clock::time_point expires;
    };

    std::mutex mutex_;
    std::array<std::optional<Entry>, Capacity> slots_{};
};

} // namespace study_meta
