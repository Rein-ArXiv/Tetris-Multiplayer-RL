#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// 루프 소유의 게임 연결을 건드리는 후속 작업은 drain 이후 루프가 실행한다.
// 워커의 HTTP 소켓은 워커가 사용할 수 있다. 게임 연결 소켓과 소유권을 구분한다.
// submit/drain/shutdown은 같은 소유자가 순차 호출한다. 워커에서 shutdown하지 않는다.
// capacity는 대기+실행+미회수 결과의 개수이며 바이트/실행 시간 상한은 아니다.
// shutdown은 수락한 작업을 마치고 join한다. 그 뒤 마지막 drain/실행을 마친 다음
// 연결/루프/외부 서비스를 파괴한다. 실행 중 연결 제거는 ID 재조회로 따로 처리한다.
// wake 대상은 모든 워커보다 오래 살아야 한다. wake 예외는 기록하고 결과는 보존한다.
// 예외 fallback을 주지 않은 작업은 루프에서 결과를 호출할 때 원래 예외를 재전파한다.

namespace study_net {

class Offload {
public:
    using Cont = std::function<void()>;  // reactor 스레드에서 실행될 후속
    using Job  = std::function<Cont()>;  // 워커 스레드에서 실행될 블로킹 작업

    struct Task {
        Job                job;
        Cont               failure;
        Cont               result;
        std::exception_ptr error;
    };

    // threads: 동시 블로킹 상한(0 이면 1). wake: 완료 시 루프를 깨우는 콜백.
    // capacity: 미완료(outstanding) 슬롯 상한 — 개수 기준(시간/바이트 아님).
    Offload(std::size_t threads, std::function<void()> wake, std::size_t capacity = 1024)
        : capacity_(capacity), wake_(std::move(wake))
    {
        if (capacity_ == 0) throw std::invalid_argument("Offload: capacity must be > 0");
        if (threads == 0) threads = 1;
        workers_.reserve(threads);
        try {
            for (std::size_t i = 0; i < threads; ++i)
                workers_.emplace_back([this] { run(); });
        } catch (...) {
            shutdown();   // 이미 시작한 워커를 조인한 뒤 예외 전파
            throw;
        }
    }

    ~Offload() { shutdown(); }  // completion 콜백은 호출하지 않음

    Offload(const Offload&)            = delete;
    Offload& operator=(const Offload&) = delete;

    // job 을 제출한다. 비었거나 stopping/용량 초과면 false.
    // 노드 할당을 outstanding 증가보다 먼저 하므로 할당 실패 시 카운트는 그대로.
    bool submit(Job job, Cont failure = {}) {
        if (!job) return false;
        Task task{std::move(job), std::move(failure), {}, {}};
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (stopping_ || outstanding_ >= capacity_) return false;
            jobs_.push_back(std::move(task));
            ++outstanding_; // 노드 할당 성공 뒤 수락한 슬롯으로 센다.
        }
        cv_.notify_one();
        return true;
    }

    // 완료된 Cont 를 out 뒤에 붙이고 옮긴 개수를 돌려준다(reactor 루프).
    // closure 는 여기서 호출하지 않는다 — 호출은 루프가 drain 이후에 한다.
    // error 노드: failure 가 있으면 옮기고, 없으면 rethrow 하는 Cont 를 만든다.
    std::size_t drain(std::vector<Cont>& out) {
        std::list<Task> retired; // lock보다 먼저 선언: 캡처 소멸은 잠금 해제 후.
        std::lock_guard<std::mutex> lk(mu_);
        const std::size_t n = done_.size();
        if (n == 0) return 0;

        if (n > out.max_size() - out.size())
            throw std::length_error("Offload::drain: vector capacity overflow");
        out.reserve(out.size() + n);  // 이동 전에 확보 — 실패하면 done_ 유지

        std::size_t moved = 0;
        while (!done_.empty()) {
            Task& t = done_.front();
            if (t.error) {
                if (t.failure) {
                    out.push_back(std::move(t.failure));
                } else {
                    // 기본 처리: 루프가 이 Cont 를 호출할 때 예외를 다시 던진다
                    Cont rethrow = [err = t.error] { std::rethrow_exception(err); };
                    out.push_back(std::move(rethrow));
                }
            } else {
                out.push_back(std::move(t.result));
            }
            retired.splice(retired.end(), done_, done_.begin()); // 전달 성공 뒤 회수
            --outstanding_;
            ++moved;
        }
        return moved;
    }

    // 새 job 을 막고 수락된 대기 job 을 마저 처리한 뒤 워커를 조인한다.
    // reactor 루프 스레드에서만 호출, 중복 호출은 무해(idempotent, 순차 전제).
    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) if (t.joinable()) t.join();
        workers_.clear();
    }

    // wake 콜백에서 새던 예외 누적치를 0 으로 교환해 돌려준다.
    // wake 실패는 이미 큐에 들어간 완료 결과를 제거하지 않는다(계속 drain 가능).
    std::size_t take_wake_errors() noexcept {
        return wake_errors_.exchange(0, std::memory_order_relaxed);
    }

private:
    void run() {
        std::list<Task> running;      // 로컬 리스트 — splice 로 노드만 이동(할당 없음)
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this] { return stopping_ || !jobs_.empty(); });
                if (jobs_.empty()) {
                    if (stopping_) return;
                    continue;
                }
                running.splice(running.end(), jobs_, jobs_.begin());
            }

            Task& t = running.back();
            try {
                t.result = t.job();   // 잠금 밖에서 블로킹 실행
            } catch (...) {
                t.error = std::current_exception();
            }
            t.job = nullptr;          // 잠금 밖에서 job 해제

            const bool publish = static_cast<bool>(t.result) || static_cast<bool>(t.error);
            bool wake = false;
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (publish) {
                    done_.splice(done_.end(), running, running.begin());
                    wake = true;
                } else {
                    --outstanding_; // 빈 성공은 전달할 결과가 없어 즉시 슬롯 반환
                }
            }
            running.clear(); // 빈 성공의 failure 캡처도 잠금 밖에서 파괴한다.
            if (wake && wake_) {      // 잠금 밖에서 wake 호출
                try {
                    wake_();
                } catch (...) {
                    wake_errors_.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    std::mutex                    mu_;
    std::condition_variable       cv_;
    std::list<Task>               jobs_;   // 대기 job (제출 순서)
    std::list<Task>               done_;   // 배출 대기 완료 노드
    std::size_t                   outstanding_ = 0;
    std::size_t                   capacity_;
    bool                          stopping_    = false;
    std::function<void()>         wake_;
    std::atomic<std::size_t>      wake_errors_{0};
    std::vector<std::thread>      workers_;
};

} // namespace study_net