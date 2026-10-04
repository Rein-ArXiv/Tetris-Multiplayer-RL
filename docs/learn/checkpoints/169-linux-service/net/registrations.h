#ifndef STUDY_NET_REGISTRATIONS_H
#define STUDY_NET_REGISTRATIONS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/socket.h"

namespace study_net {

// A Registration is an opaque, monotonically issued identity for a registered
// descriptor. It is never recycled while this Registrations object lives, so an
// older batch cannot accidentally name a new registration when the fd is reused.
// A removed id is stale and must be discarded at dispatch.
using Registration = std::uint64_t;

// Id 0 is reserved as "invalid" and also marks an exhausted id space.
inline constexpr Registration kInvalidRegistration = 0;

enum Interest : unsigned {
    Read = 1u,
    Write = 2u,
};

inline constexpr unsigned kInterestMask =
    static_cast<unsigned>(Interest::Read) | static_cast<unsigned>(Interest::Write);

// One live registration: a stable id bound to a descriptor and interest mask.
struct RegistrationEntry {
    Registration id;
    Runtime::Native fd;
    unsigned interest;
};

// Fixed-capacity, single-threaded registry of descriptor registrations.
// It owns no descriptors, performs no socket I/O and does not allocate; it only
// hands out stable ids and tracks per-fd interest. Not for concurrent use.
class Registrations {
public:
    static constexpr std::size_t capacity = 16;

    using Storage = std::array<std::optional<RegistrationEntry>, capacity>;

    // first is the first id handed out; first == 0 starts permanently exhausted.
    explicit Registrations(Registration first = 1) noexcept : next_(first) {}

    Registrations(const Registrations&) = delete;
    Registrations& operator=(const Registrations&) = delete;

    // Register fd with interest and return its stable id, or nullopt if the fd
    // is invalid, interest has unknown bits, fd is already registered, the table
    // is full, or the id space is exhausted. interest == 0 is allowed and means
    // the registration exists but is paused.
    std::optional<Registration> add(Runtime::Native fd, unsigned interest) noexcept {
        if (!Runtime::native_valid(fd)) {
            return std::nullopt;
        }
        if ((interest & ~kInterestMask) != 0) {
            return std::nullopt;
        }
        if (has_fd(fd)) {
            return std::nullopt;
        }
        std::optional<RegistrationEntry>* slot = vacant();
        if (slot == nullptr) {
            return std::nullopt;
        }
        if (next_ == kInvalidRegistration) {
            return std::nullopt;
        }
        const Registration id = next_;
        // Advance one step, but never wrap back into valid ids: 0 is permanent
        // exhaustion so ids are never reused for the lifetime of this object.
        next_ = (next_ == UINT64_MAX) ? kInvalidRegistration : next_ + 1;
        slot->emplace(RegistrationEntry{id, fd, interest});
        return id;
    }

    // Replace the interest mask of an existing registration. Returns false for
    // unknown interest bits or an unknown id.
    bool modify(Registration id, unsigned interest) noexcept {
        if ((interest & ~kInterestMask) != 0) {
            return false;
        }
        RegistrationEntry* entry = mutable_find(id);
        if (entry == nullptr) {
            return false;
        }
        entry->interest = interest;
        return true;
    }

    // Drop a registration. Only the slot is cleared; the id counter is never
    // rolled back, so ids stay unique even after removal.
    bool remove(Registration id) noexcept {
        for (auto& slot : slots_) {
            if (slot.has_value() && slot->id == id) {
                slot.reset();
                return true;
            }
        }
        return false;
    }

    // Return the entry for id, or nullptr. The pointer is valid only until the
    // entry is removed or the object is otherwise mutated.
    const RegistrationEntry* find(Registration id) const noexcept {
        for (const auto& slot : slots_) {
            if (slot.has_value() && slot->id == id) {
                return &*slot;
            }
        }
        return nullptr;
    }

    // Read-only access to the underlying fixed-size table.
    const Storage& entries() const noexcept { return slots_; }

private:
    std::optional<RegistrationEntry>* vacant() noexcept {
        for (auto& slot : slots_) {
            if (!slot.has_value()) {
                return &slot;
            }
        }
        return nullptr;
    }

    RegistrationEntry* mutable_find(Registration id) noexcept {
        for (auto& slot : slots_) {
            if (slot.has_value() && slot->id == id) {
                return &*slot;
            }
        }
        return nullptr;
    }

    bool has_fd(Runtime::Native fd) const noexcept {
        for (const auto& slot : slots_) {
            if (slot.has_value() && slot->fd == fd) {
                return true;
            }
        }
        return false;
    }

    Storage slots_{};
    Registration next_;
};

}  // namespace study_net

#endif  // STUDY_NET_REGISTRATIONS_H
