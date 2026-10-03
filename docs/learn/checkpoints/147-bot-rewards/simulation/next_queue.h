#pragma once

// Fixed-capacity FIFO of catalog Kind values for the upcoming-piece queue.
// Validates kinds against the canonical catalog but never chooses/generates them.
// No heap: all storage is inline; every operation is O(1) and non-allocating.

#include <array>
#include <cstddef>
#include <optional>

#include "simulation/catalog.h"

namespace study_next {

using Kind = study_catalog::Kind;

class Queue {
public:
    static constexpr std::size_t capacity = 3;

    // Number of live entries. Size disambiguates empty vs full; head_ alone cannot.
    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    bool full() const noexcept { return size_ == capacity; }

    // Appends to the logical back. Rejects an unknown catalog Kind or a full
    // queue WITHOUT mutating state, so a rejected value is never stored.
    bool push(Kind kind) noexcept {
        if (study_catalog::find(kind) == nullptr || full()) {
            return false;
        }
        slots_[(head_ + size_) % capacity] = kind;
        ++size_;
        return true;
    }

    // Reads the logical element at offset index (0 == front) by value.
    // Rejects index >= size_ before any wrap arithmetic, so SIZE_MAX and other
    // out-of-range offsets cannot alias a live slot.
    std::optional<Kind> peek(std::size_t index = 0) const noexcept {
        if (index >= size_) {
            return std::nullopt;
        }
        return slots_[(head_ + index) % capacity];
    }

    // Removes the logical front and returns it by value. Leaves slots_ and all
    // surviving entries physically in place; only head_ and size_ advance.
    std::optional<Kind> pop() noexcept {
        if (empty()) {
            return std::nullopt;
        }
        const Kind kind = slots_[head_];
        head_ = (head_ + 1) % capacity;
        --size_;
        return kind;
    }

private:
    // Physical storage ring. Logical order starts at head_ and runs size_ long,
    // so slot (head_ + i) % capacity holds logical element i. All three slots
    // are usable; no sentinel is reserved and no allocation occurs.
    std::array<Kind, capacity> slots_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};

}  // namespace study_next
