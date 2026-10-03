#pragma once
// Tracks task execution AND callable/capture destruction, not OS thread exit.
// Every accepted launch creates a thread; this is not a reusable thread pool.
// Call stop_accepting, signal blocked tasks to finish, then wait before
// destroying borrowed state. stop_accepting itself does not cancel a task.
// A task must not wait on its own group. External API calls must have ended
// before group destruction. TLS destructors are outside this drain boundary.
// Shared captures release their references; other owners can keep data alive.
// Task destructors must not throw. Unexpected mutex/join failures are fail-fast
// through noexcept. A failed detach falls back to a possibly blocking join.

#include <condition_variable>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace study_net {

class WorkerGroup {
public:
    explicit WorkerGroup(std::size_t limit) noexcept
        : limit_(limit) {}

    ~WorkerGroup() noexcept
    {
        stop_accepting();
        wait();
    }

    WorkerGroup(const WorkerGroup&) = delete;
    WorkerGroup& operator=(const WorkerGroup&) = delete;
    WorkerGroup(WorkerGroup&&) = delete;
    WorkerGroup& operator=(WorkerGroup&&) = delete;

    template <typename Fn>
    bool launch(Fn&& fn) noexcept
    {
        // Reserve a slot before constructing anything.
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!accepting_) return false;
            if (limit_ == 0 || active_ >= limit_) return false;
            ++active_;
        }

        // Named thread declared OUTSIDE the construction try: if detach()
        // throws we still own a joinable worker instead of destroying a
        // temporary, which would call std::terminate.
        std::thread worker;

        try {
            auto captured = std::make_unique<std::decay_t<Fn>>(std::forward<Fn>(fn));
            worker = std::thread(&WorkerGroup::run<std::decay_t<Fn>>, this, std::move(captured));
        } catch (...) {
            // The only place that calls finish() for a launch that did not
            // start. The reservation is released exactly once.
            finish();
            return false;
        }

        try {
            worker.detach();
        } catch (...) {
            // detach failed; the task is alive and joinable. Wait for it to
            // finish so the worker can be destroyed safely. Do NOT finish():
            // the task's own Completion decrements active_.
            worker.join();
        }
        return true;
    }

    void stop_accepting() noexcept
    {
        std::lock_guard<std::mutex> lk(mu_);
        accepting_ = false;
    }

    void wait() noexcept
    {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [this] { return active_ == 0; });
    }

    std::size_t active() const noexcept
    {
        std::lock_guard<std::mutex> lk(mu_);
        return active_;
    }

    std::size_t failed_tasks() const noexcept
    {
        std::lock_guard<std::mutex> lk(mu_);
        return failed_;
    }

private:
    struct Completion {
        WorkerGroup* owner;
        ~Completion() noexcept { owner->finish(); }
    };

    template <typename Work>
    static void run(WorkerGroup* self, std::unique_ptr<Work> captured) noexcept
    {
        // Completion first, owned second: reverse destruction order frees the
        // captured resources before finish() decrements active_.
        Completion completion{self};
        std::unique_ptr<Work> owned = std::move(captured);
        try {
            (*owned)();
        } catch (...) {
            self->count_failure();
        }
        // owned destructs here (before completion), captured is already empty.
    }

    void count_failure() noexcept
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (failed_ != (std::numeric_limits<std::size_t>::max)()) ++failed_;
    }

    void finish() noexcept
    {
        // notify_all() while holding the lock: wait() cannot return and destroy
        // cv_ between our unlock and our notify.
        std::lock_guard<std::mutex> lk(mu_);
        --active_;
        cv_.notify_all();
    }

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::size_t active_{0};
    std::size_t failed_{0};
    const std::size_t limit_;
    bool accepting_{true};
};

}  // namespace study_net
