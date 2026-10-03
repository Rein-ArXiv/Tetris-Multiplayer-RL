#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "simulation/input_mask.h"

// Tick window that stages one input record per side, indexed by a uint32 wire tick.
// Invariants:
//   * next_ is the oldest tick still open; it only ever increases.
//   * Every accepted input lands at slot (tick - next_), with 0 <= offset < capacity.
//   * host_ and peer_ hold independent copies; a slot is ready only when both
//     optional values at that offset are engaged.
//   * Tick ids never wrap: once next_ passes UINT32_MAX the object is exhausted.
namespace study_net {

// Scoped enumerators keep the window and driver result sets from colliding in
// this shared namespace.
enum class Side {
    host,
    peer
};

// Result of inserting one input record into the window.
enum class Put {
    stored,     // Newly recorded.
    duplicate,  // Identical input already recorded for this tick.
    conflict,   // A different mask is already recorded for this tick.
    stale,      // Older than the oldest open slot.
    too_far,    // Beyond the last slot of the window.
    invalid,    // Malformed side, mask, or batch arguments.
    exhausted   // The uint32 wire tick space has been fully consumed.
};

class TickInputs {
public:
    // Number of in-flight ticks held by the window.
    static constexpr std::size_t capacity = 32;

    explicit TickInputs(std::uint32_t first = 0) noexcept
        : next_(first) {}

    // Oldest tick still open, widened so it can reach UINT32_MAX + 1.
    std::uint64_t next_tick() const noexcept { return next_; }

    // True after consuming UINT32_MAX. No representable future tick remains.
    bool exhausted() const noexcept {
        return next_ > static_cast<std::uint64_t>(UINT32_MAX);
    }

    // Stage one input record. Validation order is deliberate:
    //   1. reject an unknown side or a mask with unknown bits, before narrowing;
    //   2. reject an exhausted tick space;
    //   3. reject a tick older than the window (stale);
    //   4. reject a tick past the window (too_far).
    // An all-zero mask is a real, accepted input (neutral), never missing.
    Put put(Side side, std::uint32_t tick, unsigned mask) noexcept {
        if (side != Side::host && side != Side::peer) {
            return Put::invalid;
        }
        if (!study_input::valid(mask)) {
            return Put::invalid;
        }
        if (exhausted()) {
            return Put::exhausted;
        }

        const std::uint64_t wire = static_cast<std::uint64_t>(tick);
        if (wire < next_) {
            return Put::stale;
        }
        const std::uint64_t offset = wire - next_;
        if (offset >= static_cast<std::uint64_t>(capacity)) {
            return Put::too_far;
        }

        std::array<std::optional<std::uint8_t>, capacity>& slots = slots_for(side);
        const std::size_t index = static_cast<std::size_t>(offset);
        if (slots[index].has_value()) {
            // Preserve what was recorded: only an identical input is harmless.
            return *slots[index] == static_cast<std::uint8_t>(mask) ? Put::duplicate
                                                                   : Put::conflict;
        }
        slots[index] = static_cast<std::uint8_t>(mask);
        return Put::stored;
    }

    // Both sides' slot 0 must be present. The caller must pass two distinct
    // variables; on false neither output is modified.
    bool peek(std::uint8_t& host, std::uint8_t& peer) const noexcept {
        if (!host_[0].has_value() || !peer_[0].has_value()) {
            return false;
        }
        host = *host_[0];
        peer = *peer_[0];
        return true;
    }

    // Retire slot 0 and advance the window by exactly one tick. Requires both
    // sides present; no input is synthesized for a missing side. The O(32)
    // left shift is intentionally simple.
    bool consume() noexcept {
        if (!host_[0].has_value() || !peer_[0].has_value()) {
            return false;
        }
        for (std::size_t i = 0; i + 1 < capacity; ++i) {
            host_[i] = host_[i + 1];
            peer_[i] = peer_[i + 1];
        }
        host_[capacity - 1].reset();
        peer_[capacity - 1].reset();
        ++next_;
        return true;
    }

    // Stage a contiguous run of input records [first, first + count). The batch is
    // all-or-nothing: any rejected input leaves the window untouched.
    // masks is caller-owned scratch and is never retained.
    Put put_batch(Side side, std::uint32_t first, const std::uint8_t* masks,
                  std::size_t count) noexcept {
        if (side != Side::host && side != Side::peer) {
            return Put::invalid;
        }
        if (masks == nullptr) {
            return Put::invalid;
        }
        if (count == 0 || count > capacity) {
            return Put::invalid;
        }
        // The run must not wrap past the end of the uint32 tick space.
        if (static_cast<std::uint64_t>(first) + (count - 1) >
            static_cast<std::uint64_t>(UINT32_MAX)) {
            return Put::invalid;
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (!study_input::valid(masks[i])) {
                return Put::invalid;
            }
        }

        // Commit-or-rollback: build the result on a copy, assign only on success.
        TickInputs candidate = *this;
        bool any_stored = false;
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint32_t tick =
                static_cast<std::uint32_t>(static_cast<std::uint64_t>(first) + i);
            const Put result = candidate.put(side, tick, masks[i]);
            if (result == Put::stored) {
                any_stored = true;
            } else if (result != Put::duplicate) {
                return result; // Whole batch rejected; *this is unchanged.
            }
        }
        *this = candidate;
        return any_stored ? Put::stored : Put::duplicate;
    }

private:
    std::array<std::optional<std::uint8_t>, capacity>& slots_for(Side side) noexcept {
        return side == Side::host ? host_ : peer_;
    }

    std::array<std::optional<std::uint8_t>, capacity> host_;
    std::array<std::optional<std::uint8_t>, capacity> peer_;
    std::uint64_t next_ = 0;
};

} // namespace study_net
