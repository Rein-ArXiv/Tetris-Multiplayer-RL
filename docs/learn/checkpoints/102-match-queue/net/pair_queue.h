#ifndef STUDY_NET_PAIR_QUEUE_H
#define STUDY_NET_PAIR_QUEUE_H

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace study_net {

// enqueue() outcome. Any value other than queued leaves the caller's T untouched.
enum class QueueJoin {
    queued,
    closed,
    invalid_id,
    duplicate,
    full,
};

// Fixed-capacity, single-mutex FIFO matchmaking queue.
//
// FIFO order is the order in which enqueue() successfully acquires the mutex,
// not the order in which connections were accepted or threads started.
//
// Slots use fixed storage; T and synchronization may own other resources.
// The container never reads sockets. Matching policy is supplied by
// the caller through a non-blocking, noexcept Poll callback.
//
// Threading contract:
//   * The user must ensure every thread that may call into the queue has
//     returned (joined/drained) before the queue is destroyed. close() alone
//     does not wait for consumers to exit.
//   * Poll is invoked while the internal mutex is held: it must be
//     non-blocking, must not re-enter this queue, and must do bounded work.
//   * T destruction happens under the mutex and must therefore be
//     non-blocking and must not re-enter the queue.
//
// A fixed capacity bounds this container only. It does not bound kernel memory
// or connection counts held elsewhere by the caller.
template <class T, std::size_t Capacity>
class PairQueue {
    static_assert(Capacity >= 2, "PairQueue requires Capacity >= 2");
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "T must be nothrow move constructible");
    static_assert(std::is_nothrow_move_assignable_v<T>,
                  "T must be nothrow move assignable");
    static_assert(std::is_nothrow_destructible_v<T>,
                  "T must be nothrow destructible");

public:
    struct Entry {
        std::uint64_t id;
        T value;
    };

    struct Pair {
        Entry first;
        Entry second;
    };

    PairQueue() = default;

    PairQueue(const PairQueue&) = delete;
    PairQueue& operator=(const PairQueue&) = delete;
    PairQueue(PairQueue&&) = delete;
    PairQueue& operator=(PairQueue&&) = delete;

    // Caller must close() and join/drain all users before destruction.
    ~PairQueue() = default;

    // Enqueue a new pending entry.
    //
    // Precedence of failures: closed, invalid_id (id == 0), duplicate (among
    // the current pending entries only), full. On any failure value is left
    // untouched. On success ownership of value is moved into the queue and
    // every waiter is notified while the mutex is still held.
    //
    // The caller must not reuse IDs while an old request might still refer to them;
    // duplicate detection applies to pending entries only.
    QueueJoin enqueue(std::uint64_t id, T& value) {
        std::lock_guard<std::mutex> lk(mu_);
        if (closed_) return QueueJoin::closed;
        if (id == 0) return QueueJoin::invalid_id;
        for (std::size_t i = 0; i < count_; ++i) {
            if (slots_[i]->id == id) return QueueJoin::duplicate;
        }
        if (count_ == Capacity) return QueueJoin::full;

        slots_[count_].emplace(Entry{id, std::move(value)});
        ++count_;
        cv_.notify_all();
        return QueueJoin::queued;
    }

    // Remove a pending entry by id. Cancellation applies only to entries that
    // are still pending; there is no match-phase cancellation. On success the
    // removed entry is returned with ownership, survivor order is preserved,
    // and waiters are notified. If nothing matches, the queue is unchanged and
    // nullopt is returned.
    std::optional<Entry> cancel(std::uint64_t id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (std::size_t i = 0; i < count_; ++i) {
            if (slots_[i]->id == id) {
                std::optional<Entry> removed = take_at_locked(i);
                cv_.notify_all();
                return removed;
            }
        }
        return std::nullopt;
    }

    // Does not wait for a partner, but acquiring mu_ can block.
    // Scans every pending entry with poll(); an
    // entry for which poll() returns false is removed (and destroyed) here,
    // while the mutex is held. If at least two entries survive, the first two
    // are removed and returned as a Pair; otherwise nullopt is returned.
    //
    // Poll must be non-blocking and noexcept, must not re-enter this queue, and
    // must do bounded work. The pure container never reads sockets; external
    // policy supplies liveness. The poll-vs-match linearization happens
    // entirely under the mutex, which is what arbitrates the race.
    template <class Poll>
    std::optional<Pair> try_pair(Poll&& poll) {
        static_assert(
            std::is_nothrow_invocable_r_v<bool, Poll&, std::uint64_t, T&>,
            "Poll must be noexcept-invocable as bool(std::uint64_t, T&)");
        std::lock_guard<std::mutex> lk(mu_);
        if (closed_) return std::nullopt;

        prune_locked(poll);
        if (count_ < 2) return std::nullopt;

        Entry a = take_at_locked(0);
        Entry b = take_at_locked(0);
        return std::optional<Pair>{Pair{std::move(a), std::move(b)}};
    }

    // Blocking variant: same scan/match as try_pair(), retried in a loop.
    //
    //   * zero entries : wait on the predicate (closed_ || count_ != 0)
    //   * one entry    : wait_for(50ms) so a lonely entry is re-polled even if
    //                    no notification arrives; enqueue() notifies at once
    //   * spurious wakeups are harmless: the loop re-scans every time.
    //
    // There is no spin at count == 1 and no detached helper thread. notify is
    // not treated as permission: the predicate is always re-checked under the
    // mutex. All pending entries are scanned each pass.
    template <class Poll>
    std::optional<Pair> wait_pair(Poll&& poll) {
        static_assert(
            std::is_nothrow_invocable_r_v<bool, Poll&, std::uint64_t, T&>,
            "Poll must be noexcept-invocable as bool(std::uint64_t, T&)");
        std::unique_lock<std::mutex> lk(mu_);
        for (;;) {
            if (closed_) return std::nullopt;

            prune_locked(poll);
            if (count_ >= 2) {
                Entry a = take_at_locked(0);
                Entry b = take_at_locked(0);
                return std::optional<Pair>{Pair{std::move(a), std::move(b)}};
            }

            if (count_ == 0) {
                cv_.wait(lk, [this] { return closed_ || count_ != 0; });
            } else {
                cv_.wait_for(lk, std::chrono::milliseconds(50));
            }
        }
    }

    // Idempotent. Marks the queue closed, destroys all pending entries, and
    // wakes every waiter while still holding the mutex.
    void close() {
        std::lock_guard<std::mutex> lk(mu_);
        if (closed_) return;
        closed_ = true;
        for (std::size_t i = 0; i < count_; ++i) {
            slots_[i].reset();
        }
        count_ = 0;
        cv_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mu_);
        return count_;
    }

    bool closed() const {
        std::lock_guard<std::mutex> lk(mu_);
        return closed_;
    }

private:
    using Slot = std::optional<Entry>;

    // Remove slots_[idx] and return ownership of the removed entry. Survivors
    // keep their relative order: each following slot is moved down into the
    // already-moved-from slot before the tail is reset. Requires only nothrow
    // move assignment, so move-only T is fine; T is never copied.
    Entry take_at_locked(std::size_t idx) {
        Entry out = std::move(*slots_[idx]);
        for (std::size_t i = idx + 1; i < count_; ++i) {
            slots_[i - 1] = std::move(slots_[i]);
        }
        slots_[count_ - 1].reset();
        --count_;
        return out;
    }

    // Drop every entry for which poll() returns false. Removed entries are
    // destroyed here, while the mutex is held.
    template <class Poll>
    void prune_locked(Poll& poll) {
        std::size_t i = 0;
        while (i < count_) {
            if (poll(slots_[i]->id, slots_[i]->value)) {
                ++i;
            } else {
                take_at_locked(i);
            }
        }
    }

    mutable std::mutex mu_;
    std::condition_variable cv_;
    bool closed_ = false;
    std::size_t count_ = 0;
    std::array<Slot, Capacity> slots_{};  // dense prefix [0, count_)
};

}  // namespace study_net

#endif  // STUDY_NET_PAIR_QUEUE_H
