#ifndef STUDY_HANDLES_HANDLE_POOL_H
#define STUDY_HANDLES_HANDLE_POOL_H

// study_handles::HandlePool<T>
//
// A render-thread-only pool of opaque handles for image resources. It is
// deliberately single-threaded: no mutexes, no atomics, no serialization.
// A handle is never a pointer, a GL name or a security token and must never be
// persisted or interpreted as one.
//
// Borrowed pointers returned by find() stay valid when the backing vector grows
// (slots hold unique_ptr), but expire on erase()/clear() or pool destruction.
// The caller must not delete a borrowed pointer. Resource destructors and
// callbacks must not reenter or mutate this pool while an operation is active.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace study_handles {

// Opaque resource handle. Zero is always invalid.
using Handle = std::uint64_t;

// Process-local stamp shared by every HandlePool<T> instantiation and instance.
// Stamps are never wrapped or reused: once this counter is driven to zero via
// UINT32_MAX the stamp space is exhausted for the lifetime of the process.
inline std::uint32_t next_stamp = 1;

// Returns the current stamp then advances `next`. UINT32_MAX is the final usable
// stamp and leaves `next` at zero (exhausted); a zero `next` stays zero.
inline std::uint32_t take_stamp(std::uint32_t& next) noexcept {
    std::uint32_t const current = next;
    if (current == 0u || current == (std::numeric_limits<std::uint32_t>::max)()) {
        next = 0u;
    } else {
        next = current + 1u;
    }
    return current;
}

// Handle layout helpers. High 32 bits: stamp. Low 32 bits: slot + 1.
inline Handle make_handle(std::uint32_t stamp, std::uint32_t slot_plus_one) noexcept {
    return (static_cast<Handle>(stamp) << 32) | static_cast<Handle>(slot_plus_one);
}

template <class T>
class HandlePool final {
    static_assert(std::is_nothrow_destructible<T>::value,
                  "HandlePool<T> requires T to be nothrow destructible");

public:
    HandlePool() = default;
    HandlePool(HandlePool const&) = delete;
    HandlePool& operator=(HandlePool const&) = delete;
    HandlePool(HandlePool&&) = delete;
    HandlePool& operator=(HandlePool&&) = delete;
    ~HandlePool() = default;

    // Takes ownership of `value` and returns a fresh handle, or zero on failure.
    // `value` is consumed on every outcome: stored on success, destroyed
    // otherwise. A null `value` always yields zero.
    Handle insert(std::unique_ptr<T> value) noexcept {
        if (!value || next_stamp == 0u) {
            return 0u;
        }

        // First empty slot wins; otherwise a new slot is appended.
        std::size_t index = slots_.size();
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i].value) {
                index = i;
                break;
            }
        }

        // The low 32 bits hold slot + 1, which must remain representable. The
        // value is never zero, and slot + 1 fits while slot <= UINT32_MAX - 1.
        if (index > static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)() - 1u)) {
            return 0u;
        }

        // Grow the vector before consuming a stamp so a failed allocation cannot
        // waste one.
        if (index == slots_.size()) {
            try {
                slots_.emplace_back();
            } catch (std::bad_alloc const&) {
                return 0u;
            } catch (std::length_error const&) {
                return 0u;
            }
        }

        std::uint32_t const stamp = take_stamp(next_stamp);
        if (stamp == 0u) {
            return 0u; // stamp space exhausted; `value` is destroyed on return
        }

        // From here on nothing throws.
        Slot& slot = slots_[index];
        slot.stamp = stamp;
        slot.value = std::move(value);
        ++live_count_;
        return make_handle(stamp, static_cast<std::uint32_t>(index + 1u));
    }

    // Returns a borrowed pointer to the live resource, or nullptr. The caller
    // must not delete it. The pointer stays valid across vector growth but
    // expires on erase()/clear() or destruction.
    T* find(Handle handle) noexcept {
        Slot* slot = lookup(handle);
        return slot ? slot->value.get() : nullptr;
    }

    T const* find(Handle handle) const noexcept {
        Slot const* slot = lookup(handle);
        return slot ? slot->value.get() : nullptr;
    }

    // Destroys the resource for `handle`. Returns false when the handle is not
    // currently valid.
    bool erase(Handle handle) noexcept {
        Slot* slot = lookup(handle);
        if (!slot) {
            return false;
        }
        slot->stamp = 0u; // invalidate before releasing the value
        slot->value.reset();
        --live_count_;
        return true;
    }

    // Destroys every resource and drops slot metadata. The process stamp counter
    // is intentionally left unchanged so handles are never reused.
    void clear() noexcept {
        for (Slot& slot : slots_) {
            slot.stamp = 0u;
            slot.value.reset();
        }
        slots_.clear();
        live_count_ = 0u;
    }

    std::size_t size() const noexcept { return live_count_; }

    // Invokes `fn` with each live T&. The callback must not mutate this pool.
    template <class Function>
    void for_each(Function&& fn) {
        for (Slot& slot : slots_) {
            if (slot.value) {
                fn(*slot.value);
            }
        }
    }

private:
    struct Slot {
        std::uint32_t stamp = 0u;
        std::unique_ptr<T> value{};
    };

    // Validates both nonzero halves, the vector bounds and the stamp, and
    // returns the slot only when it currently holds a live value.
    Slot* lookup(Handle handle) noexcept {
        std::size_t index = 0;
        std::uint32_t stamp = 0;
        if (!decode(handle, index, stamp) || index >= slots_.size()) {
            return nullptr;
        }
        Slot& slot = slots_[index];
        if (slot.stamp != stamp || !slot.value) {
            return nullptr;
        }
        return &slot;
    }

    Slot const* lookup(Handle handle) const noexcept {
        std::size_t index = 0;
        std::uint32_t stamp = 0;
        if (!decode(handle, index, stamp) || index >= slots_.size()) {
            return nullptr;
        }
        Slot const& slot = slots_[index];
        if (slot.stamp != stamp || !slot.value) {
            return nullptr;
        }
        return &slot;
    }

    static bool decode(Handle handle, std::size_t& index, std::uint32_t& stamp) noexcept {
        std::uint32_t const low = static_cast<std::uint32_t>(handle & 0xFFFFFFFFu);
        std::uint32_t const high = static_cast<std::uint32_t>(handle >> 32);
        if (low == 0u || high == 0u) {
            return false;
        }
        index = static_cast<std::size_t>(low - 1u);
        stamp = high;
        return true;
    }

    std::vector<Slot> slots_;
    std::size_t live_count_ = 0u;
};

} // namespace study_handles

#endif // STUDY_HANDLES_HANDLE_POOL_H