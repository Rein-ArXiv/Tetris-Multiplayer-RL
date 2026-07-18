#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

namespace relay {

// Tracks detached workers so their owner can stop accepting new work and wait
// until every running callback has released its references.
class WorkerGroup {
public:
    explicit WorkerGroup(const char* name) noexcept : name_(name) {}

    ~WorkerGroup()
    {
        stopAccepting();
        wait();
    }

    WorkerGroup(const WorkerGroup&) = delete;
    WorkerGroup& operator=(const WorkerGroup&) = delete;

    template <typename Fn>
    bool launch(Fn&& fn) noexcept
    {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!accepting_) return false;
            ++active_;
        }

        try {
            std::thread([this, work = std::forward<Fn>(fn)]() mutable {
                Completion completion{this};
                try {
                    work();
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "[%s] worker failed: %s\n", name_, e.what());
                } catch (...) {
                    std::fprintf(stderr, "[%s] worker failed: unknown exception\n", name_);
                }
            }).detach();
        } catch (const std::exception& e) {
            finish();
            std::fprintf(stderr, "[%s] worker launch failed: %s\n", name_, e.what());
            return false;
        } catch (...) {
            finish();
            std::fprintf(stderr, "[%s] worker launch failed: unknown exception\n", name_);
            return false;
        }
        return true;
    }

    void stopAccepting() noexcept
    {
        std::lock_guard<std::mutex> lk(mu_);
        accepting_ = false;
    }

    void wait() noexcept
    {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [this] { return active_ == 0; });
    }

private:
    struct Completion {
        WorkerGroup* owner;
        ~Completion() { owner->finish(); }
    };

    void finish() noexcept
    {
        // notify 는 반드시 lock 보유 중에 — unlock 후 notify 하면, 그 사이에
        // wait() 쪽이 spurious wakeup 으로 active_==0 을 보고 반환해 cv_ 를
        // 파괴한 뒤 (예: ~WorkerGroup) 이 스레드가 파괴된 cv_ 에 notify 하는
        // use-after-free 경합이 생긴다. lock 안이면 waiter 는 lock 재획득
        // 전까지 반환할 수 없어 cv_ 수명이 보장된다.
        std::lock_guard<std::mutex> lk(mu_);
        --active_;
        cv_.notify_all();
    }

    const char* name_;
    std::mutex mu_;
    std::condition_variable cv_;
    size_t active_{0};
    bool accepting_{true};
};

}  // namespace relay
