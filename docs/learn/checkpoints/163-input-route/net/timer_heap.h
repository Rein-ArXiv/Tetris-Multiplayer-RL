#ifndef STUDY_NET_TIMER_HEAP_H
#define STUDY_NET_TIMER_HEAP_H
#include "net/deadline_wait.h"
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// 단일 루프가 소유한다. 다른 스레드의 arm/cancel 호출은 지원하지 않는다.
namespace study_net {

class TimerHeap {
public:
    using Token = std::uint64_t; // 한 Reactor 인스턴스가 발급한 등록 번호
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // token 의 데드라인을 when 으로 설정/교체한다. 이미 있으면 이전 것은 무효화된다.
    void arm(Token token, TimePoint when) {
        if (token == 0) throw std::invalid_argument("invalid timer token");
        if (next_seq_ == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("timer sequence exhausted");
        const std::uint64_t seq = ++next_seq_;  // 실패한 시도의 번호도 재사용하지 않는다.
        heap_.push(Entry{when, token, seq});
        // 힙 삽입 성공 후 공개한다. 할당 실패 시 기존 유효 만기는 유지된다.
        live_[token] = seq;
    }

    // 아직 큐에 있는 만기를 취소한다. 이미 out 으로 반환한 결과는 철회하지 않는다.
    void cancel(Token token) {
        live_.erase(token);
    }

    // 유효 만기가 없으면 -1. 상위 루프는 종료 확인 주기와 별도로 최솟값을 취한다.
    int timeout_ms(TimePoint now) {
        prune_stale();
        if (heap_.empty()) return -1;
        return deadline_wait_ms(now, heap_.top().when);
    }

    // now 까지 도래한 최신 token 을 out 뒤에 추가한다. out 은 호출자가 비운다.
    // 반환 결과는 일회성: 반복은 다시 arm. token 대상의 생존 여부는 상위에서 검사한다.
    void expired(TimePoint now, std::vector<Token>& out) {
        while (!heap_.empty() && heap_.top().when <= now) {
            Entry e = heap_.top();
            auto it = live_.find(e.token);
            if (it != live_.end() && it->second == e.seq) {
                out.push_back(e.token); // 할당 실패해도 이 항목은 아직 힙/맵에 있다.
                live_.erase(it);  // 한 번 발화하고 소진
            }
            heap_.pop(); // seq 불일치인 낡은 항목도 제거한다.
        }
    }

    bool empty() {
        prune_stale();
        return heap_.empty();
    }

private:
    struct Entry {
        TimePoint when;
        Token     token;
        std::uint64_t  seq;
    };
    struct Later {
        // 이른 만기 우선, 같은 시각은 arm 순서. 포인터 대소 비교는 쓰지 않는다.
        bool operator()(const Entry& a, const Entry& b) const {
            return a.when != b.when ? a.when > b.when : a.seq > b.seq;
        }
    };

    // 힙 앞쪽의 낡은(무효화된) 항목을 걷어낸다. timeout_ms/empty 가 정확한 top 을
    // 보게 한다.
    void prune_stale() {
        while (!heap_.empty()) {
            const Entry& e = heap_.top();
            auto it = live_.find(e.token);
            if (it != live_.end() && it->second == e.seq) break;  // 최신 — 유효
            heap_.pop();
        }
    }

    std::priority_queue<Entry, std::vector<Entry>, Later> heap_;
    std::unordered_map<Token, std::uint64_t> live_;
    std::uint64_t next_seq_ = 0;
};

} // namespace study_net

#endif
