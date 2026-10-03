#ifndef STUDY_BAG_SEVEN_BAG_H
#define STUDY_BAG_SEVEN_BAG_H

#include <array>
#include <cstddef>
#include <optional>

#include "simulation/catalog.h"

namespace study_bag {

// Deterministic seven-bag mechanism only. Choice quality (RNG) is kept
// separate: this class owns the bag property, not the sampling.
class SevenBag {
public:
    using Kind = study_catalog::Kind;

    static constexpr std::size_t capacity = 7;
    static_assert(study_catalog::definitions.size() == capacity,
                  "catalog must enumerate exactly seven pieces");

    SevenBag() noexcept { refill(); }

    // Number of live slots in the active prefix.
    std::size_t remaining() const noexcept { return count_; }

    // Valid draw indices are [0, next_bound()). Empty refills on a successful draw.
    std::size_t next_bound() const noexcept {
        return count_ ? count_ : capacity;
    }

    // Active prefix only; the array tail is never logically part of the bag.
    std::optional<Kind> at(std::size_t index) const noexcept {
        if (index >= count_) {
            return std::nullopt;
        }
        return slots_[index];
    }

    // Stable erase: chosen slot is copied, later active slots shift left by one.
    // Invalid indices are rejected before any refill so state stays untouched.
    std::optional<Kind> take(std::size_t index) noexcept {
        if (index >= capacity || (count_ != 0 && index >= count_)) {
            return std::nullopt;  // no refill, no mutation
        }
        if (count_ == 0) {
            refill();
        }
        const Kind chosen = slots_[index];
        for (std::size_t i = index; i + 1 < count_; ++i) {
            slots_[i] = slots_[i + 1];
        }
        --count_;
        return chosen;
    }

private:
    void refill() noexcept {
        for (std::size_t i = 0; i < capacity; ++i) {
            slots_[i] = study_catalog::definitions[i].kind;
        }
        count_ = capacity;
    }

    std::array<Kind, capacity> slots_{};
    std::size_t count_ = 0;
};

}  // namespace study_bag

#endif  // STUDY_BAG_SEVEN_BAG_H
