#pragma once
#include <chrono>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// server/timer_queue.h — 이벤트 루프용 만기(deadline) 타이머 힙
//
// 왜 필요한가
//   스레드 모델에서는 각 연결 스레드가 sleep_for 폴링 사이에 now() >= deadline 을
//   직접 비교했다. 단일 reactor 루프에는 그 스레드들이 없다. 대신 모든 연결의
//   데드라인(첫 프레임 5초, 방향별 idle 15초, 룸 게스트 15분/READY 60초, 큐 로비
//   30초)을 한 곳에 모아, 루프가 "다음 만기까지 몇 ms" 를 구해 reactor::poll 의
//   timeout 으로 넘기고, 깨어난 뒤 만기 지난 연결만 처리한다.
//
// 설계
//   연결마다 데드라인은 하나뿐이고 갱신(룸 호스트가 게스트 입장 시 재무장)·취소가
//   잦다. min-heap + 지연 무효화(lazy invalidation)로 O(log n) arm, O(1) 취소를
//   얻는다. 각 token 의 "살아 있는 세대(seq)"를 map 에 두고, 힙에서 꺼낸 항목의
//   seq 가 최신이 아니면 낡은 것으로 보고 버린다.
//
//   세대는 인스턴스 전역에서 단조 증가한다 — token 별로 세지 않는다. 발화·취소가
//   token 을 map 에서 지우므로 token 별 카운터였다면 다음 arm 이 1 부터 다시
//   시작하고, 힙에 남은 낡은 항목의 세대와 값이 같아져 그 낡은 항목이 유효한 것으로
//   오인된다(조기 만기 + 진짜 만기 유실). 연결 상태 객체가 파괴된 자리에 새 연결이
//   같은 주소로 할당되면 token 까지 재사용되므로 실제로 일어날 수 있는 경로다.
//
//   token 은 연결 상태 객체 포인터다(reactor 의 Event::token 과 같은 값). 타이머는
//   token 을 해석하지 않는다.
//
//   동시성: 루프 스레드 전용이다(thread-safe 가 아니다). 오프로드 워커는 데드라인을
//   직접 만지지 말고 continuation 안에서 — 즉 루프 스레드에서 — arm/cancel 해야 한다.
// ─────────────────────────────────────────────────────────────────────────────

namespace relay {

class TimerQueue {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // token 의 데드라인을 when 으로 설정/교체한다. 이미 있으면 이전 것은 무효화된다.
    void arm(void* token, TimePoint when) {
        const uint64_t seq = ++next_seq_;  // 전역 단조 — 세대 값은 재사용되지 않는다
        live_[token] = seq;
        heap_.push(Entry{when, token, seq});
    }

    // token 의 데드라인을 취소한다(만기 콜백이 오지 않게). 없으면 무해.
    void cancel(void* token) {
        live_.erase(token);
    }

    // now 기준 다음 만기까지 남은 밀리초. 없으면 -1(무기한 대기 가능). 이미 지난
    // 만기가 있으면 0. 호출 중 힙 앞의 낡은 항목을 청소한다.
    int timeout_ms(TimePoint now) {
        prune_stale();
        if (heap_.empty()) return -1;
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            heap_.top().when - now).count();
        if (diff < 0) return 0;
        // int 범위로 클램프(아주 먼 만기 방지).
        if (diff > 0x3fffffff) diff = 0x3fffffff;
        return static_cast<int>(diff);
    }

    // now 시점까지 만기가 지난 token 들을 out 에 담는다(각 token 은 소진되어 힙에서
    // 사라진다 — 반복 만기가 필요하면 호출자가 다시 arm). 살아 있는 최신 항목만 낸다.
    void expired(TimePoint now, std::vector<void*>& out) {
        while (!heap_.empty() && heap_.top().when <= now) {
            Entry e = heap_.top();
            heap_.pop();
            auto it = live_.find(e.token);
            if (it != live_.end() && it->second == e.seq) {
                out.push_back(e.token);
                live_.erase(it);  // 한 번 발화하고 소진
            }
            // seq 불일치 = 재무장/취소된 낡은 항목 — 버린다.
        }
    }

    bool empty() {
        prune_stale();
        return heap_.empty();
    }

private:
    struct Entry {
        TimePoint when;
        void*     token;
        uint64_t  seq;
    };
    struct Later {
        // priority_queue 는 최대 힙 — when 이 이른 것이 top 이 되도록 뒤집는다.
        bool operator()(const Entry& a, const Entry& b) const {
            return a.when > b.when;
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
    std::unordered_map<void*, uint64_t> live_;
    uint64_t next_seq_ = 0;
};

} // namespace relay
