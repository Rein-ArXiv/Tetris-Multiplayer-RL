#pragma once
// study_net::TransferInbox
//
// A single bounded mailbox for std::unique_ptr<T> with many producers
// and exactly one consumer.
//
// This class owns no wake-up mechanism. A producer that successfully
// publishes an object must notify the consumer separately (for example
// by posting to an event queue). A consumer that also polls periodically
// can recover a wake-up that is lost between publication and notification.

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>

namespace study_net {

template <class T, std::size_t Capacity>
class TransferInbox {
    static_assert(Capacity > 0, "TransferInbox requires Capacity > 0");

public:
    using Batch = std::array<std::unique_ptr<T>, Capacity>;

    TransferInbox() = default;
    TransferInbox(const TransferInbox&) = delete;
    TransferInbox& operator=(const TransferInbox&) = delete;

    // The destructor must run only after every producer and the single
    // consumer thread have been joined.
    ~TransferInbox() = default;

    // Try to hand ownership of `value` to the inbox.
    //
    // Returns false for a null value, a closed inbox, or a full inbox,
    // retaining ownership in the caller on failure. On success the move
    // happens while holding the mutex, which publishes the pointee writes
    // to the consumer, through the consumer acquiring the same mutex before taking ownership.
    // After a successful push the caller must not use any raw alias
    // (pointer or reference) it previously held to the object.
    bool try_push(std::unique_ptr<T>& value) {
        if (!value) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (!accepting || size == Capacity) {
            return false;
        }
        slots[size] = std::move(value);
        ++size;
        return true;
    }

    // Move every published object out of the inbox and leave it open.
    // The destination batch starts empty, so moves only assign into empty
    // std::unique_ptr slots and never destroy an object here.
    Batch take() {
        Batch out{};
        std::lock_guard<std::mutex> lock(mutex);
        for (std::size_t i = 0; i < size; ++i) {
            out[i] = std::move(slots[i]);
        }
        size = 0;
        return out;
    }

    // Close the inbox and drain it in one critical section. accepting is
    // cleared before the drain while still holding the lock, so every
    // later try_push is rejected permanently. Objects published before
    // the close are still returned.
    Batch close_and_take() {
        Batch out{};
        std::lock_guard<std::mutex> lock(mutex);
        accepting = false;
        for (std::size_t i = 0; i < size; ++i) {
            out[i] = std::move(slots[i]);
        }
        size = 0;
        return out;
    }

private:
    // No T destructor and no user callback runs while the mutex is held:
    // transfers move only into empty destinations, so nothing is destroyed
    // during a transfer. Destruction of transferred objects happens in
    // the caller's context after the lock is released.
    std::mutex mutex;
    Batch slots{};
    std::size_t size = 0;
    bool accepting = true;
};

}  // namespace study_net

