#pragma once
#include <chrono>
#include <cstdint>
#include <queue>
#include <limits>
#include <stdexcept>
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
//   잦다. min-heap + 지연 무효화(lazy invalidation)를 쓴다. 저장 항목 H개에서
//   힙 삽입 비교는 O(log H), 해시 취소는 평균 O(1). 배열 재할당과 해시 최악 비용은
//   별도이며, 낡은 항목까지 H에 포함한다. 각 token의 최신 seq를 map에 두고, 꺼낸 항목의
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
        if (next_seq_ == std::numeric_limits<uint64_t>::max())
            throw std::overflow_error("timer sequence exhausted");
        const uint64_t seq = ++next_seq_;  // 실패한 시도의 번호도 재사용하지 않는다.
        heap_.push(Entry{when, token, seq});
        // 힙 삽입 성공 후 공개한다. 할당 실패 시 기존 유효 만기는 유지된다.
        live_[token] = seq;
    }

    // 아직 큐에 있는 만기를 취소한다. 이미 out 으로 반환한 결과는 철회하지 않는다.
    void cancel(void* token) {
        live_.erase(token);
    }

    // 이미 지난 만기는 0, 미래는 밀리초 올림. 뺄셈 자체가 넘치기 전에 포화한다.
    // 지원 Clock 은 아래 최대 대기 밀리초를 정확히 표현할 수 있어야 한다.
    static int wait_ms_until(TimePoint now, TimePoint when) {
        constexpr int cap_ms = 0x3fffffff;
        constexpr auto cap = std::chrono::duration_cast<Clock::duration>(
            std::chrono::milliseconds(cap_ms));
        static_assert(std::chrono::duration_cast<std::chrono::milliseconds>(cap).count()
                      == cap_ms, "steady clock must represent timer cap exactly");
        if (when <= now) return 0;
        if (now <= TimePoint::max() - cap && when >= now + cap) return cap_ms;
        return static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(when - now).count());
    }

    // 유효 만기가 없으면 -1. 상위 루프는 종료 확인 주기와 별도로 최솟값을 취한다.
    int timeout_ms(TimePoint now) {
        prune_stale();
        if (heap_.empty()) return -1;
        return wait_ms_until(now, heap_.top().when);
    }

    // now 까지 도래한 최신 token 을 out 뒤에 추가한다. out 은 호출자가 비운다.
    // 반환 결과는 일회성: 반복은 다시 arm. token 대상의 생존 여부는 상위에서 검사한다.
    void expired(TimePoint now, std::vector<void*>& out) {
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
        void*     token;
        uint64_t  seq;
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
    std::unordered_map<void*, uint64_t> live_;
    uint64_t next_seq_ = 0;
};

} // namespace relay
