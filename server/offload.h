#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// server/offload.h — 이벤트 루프 밖에서 블로킹 작업을 돌리고 결과를 루프로 회수
//
// 왜 필요한가
//   단일 reactor 루프의 최대 위험은 처리량이 아니라 "한 핸들러가 오래 걸리면 그
//   매치만이 아니라 전원이 멈추는 것"이다. 릴레이에는 그런 지뢰가 둘 있다 —
//   인증의 meta verify_token(HTTP 3초)과 매치 종료의 post_match(HTTP 10초). 이
//   둘을 루프 스레드에서 부르면 그 왕복 동안 모든 연결의 전달이 정지한다.
//
//   Offload 는 그 블로킹 호출을 작은 워커 풀로 빼고, 결과만 reactor::wake() 로
//   루프에 되돌린다. 소켓 I/O 는 여전히 루프 스레드에만 있어야 하므로(단일 스레드
//   소켓 소유 불변식), 워커는 네트워크 결과를 계산만 하고 그 결과로 무엇을 할지
//   (MATCH_RESULT 송신 등)는 루프가 한다.
//
// 계약
//   submit(job): job 은 워커 스레드에서 실행되어 "continuation" 을 돌려준다.
//   continuation 은 job 이 계산한 결과를 포착한 클로저로, 반드시 루프 스레드에서
//   실행된다(drain 이 돌려준 것을 루프가 호출). 즉:
//       offload.submit([=]() -> Offload::Cont {
//           auto res = meta->verify_token(token, 3);      // 워커에서 블로킹
//           return [=]{ resume_conn_on_loop(conn, res); };// 루프에서 실행
//       });
//   job 완료 시 wake 콜백이 불려 루프의 poll 이 깨어나고, 루프는 drain() 으로
//   continuation 들을 걷어 순서대로 실행한다.
//
//   수명: continuation 은 연결 상태 객체를 포착할 수 있다. 종료 시 루프는 연결을
//   파기하기 전에 shutdown()(진행 중 job 완료까지 대기) → 마지막 drain() 순으로
//   비워야 use-after-free 가 없다.
// ─────────────────────────────────────────────────────────────────────────────

namespace relay {

class Offload {
public:
    using Cont = std::function<void()>;          // 루프 스레드에서 실행될 후속
    using Job  = std::function<Cont()>;          // 워커 스레드에서 실행될 블로킹 작업

    // threads: 동시 블로킹 왕복 상한. wake: 완료 시 루프를 깨우는 콜백
    // (보통 [r]{ r->wake(); } 로 reactor 에 바인딩).
    Offload(std::size_t threads, std::function<void()> wake)
        : wake_(std::move(wake))
    {
        if (threads == 0) threads = 1;
        for (std::size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] { run(); });
        }
    }

    ~Offload() { shutdown(); }

    // 블로킹 job 을 워커 풀에 제출한다. 루프 스레드에서 호출.
    void submit(Job job) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            jobs_.push_back(std::move(job));
        }
        cv_.notify_one();
    }

    // 완료된 continuation 들을 걷어 out 으로 옮긴다(루프 스레드에서 호출). wake()
    // 후에 부른다. 반환값 = 걷은 개수.
    std::size_t drain(std::vector<Cont>& out) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& c : done_) out.push_back(std::move(c));
        std::size_t n = done_.size();
        done_.clear();
        return n;
    }

    // 새 job 을 막고, 진행 중 job 이 끝날 때까지 워커를 조인한다. 이후 drain() 으로
    // 남은 continuation 을 마저 비울 수 있다(idempotent).
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

private:
    void run() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this] { return stopping_ || !jobs_.empty(); });
                if (jobs_.empty()) {
                    if (stopping_) return;
                    continue;
                }
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }
            Cont cont = job();  // 블로킹 (HTTP 등) — 루프 밖
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (cont) done_.push_back(std::move(cont));
            }
            if (wake_) wake_();  // 루프의 poll 을 깨워 drain 하게 한다
        }
    }

    std::mutex               mu_;
    std::condition_variable  cv_;
    std::deque<Job>          jobs_;
    std::vector<Cont>        done_;
    bool                     stopping_ = false;
    std::function<void()>    wake_;
    std::vector<std::thread> workers_;
};

} // namespace relay
