#pragma once

#include <mutex>
#include <condition_variable>

#include "net/worker_group.h"

namespace study_net {

// StopGate: a plain-thread-context stop flag with a condition variable used to
// wake threads that are blocked in wait().
//
// - Not copyable and not movable.
// - Public methods are for ordinary thread context only and are NOT
//   signal-safe; do not call them from a signal handler.
// - Methods are noexcept: an unexpected synchronization failure is treated as
//   fail-fast, not as a recoverable error.
// - request_stop() sets a sticky flag and wakes every waiter. It does NOT
//   cancel running I/O and does NOT join any thread.
// - The gate must outlive all users; ownership and lifetime are the caller's
//   responsibility.
class StopGate {
public:
    StopGate() = default;
    ~StopGate() = default;

    StopGate(const StopGate&) = delete;
    StopGate& operator=(const StopGate&) = delete;
    StopGate(StopGate&&) = delete;
    StopGate& operator=(StopGate&&) = delete;

    // Blocks until request_stop() has been called. Safe to call again after it
    // has returned: the sticky flag keeps the predicate true.
    void wait() noexcept {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return stopping_; });
    }

    // Sets the sticky stop flag and wakes all waiters. The flag is set before
    // notify_all(). The shared mutex and persistent predicate prevent lost wakeups.
    void request_stop() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

// ShutdownDrain: optional RAII helper that ties a StopGate to a WorkerGroup.
//
// - Construct with references to an existing WorkerGroup and StopGate.
// - finish() stops accepting new work, requests the stop, then waits for the
//   worker group to drain. It performs no allocations and is repeat-safe.
// - The destructor calls finish().
// - External launching calls must already be stopped/joined before finish() is
//   called. Do not call finish() from inside a group task.
// - Only the owner thread may control finish()/destruction, and both the gate
//   and the worker group must outlive the guard.
// - WorkerGroup::wait() waits for callbacks and capture destructors only; it
//   does not wait for complete OS thread termination or TLS destructors.
class ShutdownDrain {
public:
    ShutdownDrain(WorkerGroup& workers, StopGate& gate) noexcept
        : workers_(workers), gate_(gate) {}

    ShutdownDrain(const ShutdownDrain&) = delete;
    ShutdownDrain& operator=(const ShutdownDrain&) = delete;

    void finish() noexcept {
        workers_.stop_accepting();
        gate_.request_stop();
        workers_.wait();
    }

    ~ShutdownDrain() noexcept {
        finish();
    }

private:
    WorkerGroup& workers_;
    StopGate& gate_;
};

}  // namespace study_net
