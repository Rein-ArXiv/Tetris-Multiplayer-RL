// tests/loop_primitives_test.cpp — 이벤트 루프 원시 도구 회귀
//
// 단일 reactor 루프를 조립하기 전에, 그것이 의존하는 두 도구를 격리 검증한다:
//   - TimerQueue: 만기 순서, 재무장(re-arm), 취소, 다음 만기까지 timeout 계산
//   - Offload:    블로킹 job 을 워커에서 돌리고 continuation 을 루프 스레드로 회수,
//                 완료 시 wake 콜백 발화

#include "../server/timer_queue.h"
#include "../server/offload.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;
void check(bool cond, const char* what) {
    if (!cond) { std::fprintf(stderr, "[loop-prim] FAIL: %s\n", what); ++g_failures; }
    else       { std::fprintf(stderr, "[loop-prim] ok:   %s\n", what); }
}

using Clock = std::chrono::steady_clock;
using ms    = std::chrono::milliseconds;

void test_timer_queue() {
    relay::TimerQueue tq;
    auto base = Clock::now();
    int a, b, c;  // 토큰 대용(주소만 씀)

    // 만기 순서: b(10) < a(20) < c(30)
    tq.arm(&a, base + ms(20));
    tq.arm(&b, base + ms(10));
    tq.arm(&c, base + ms(30));

    std::vector<void*> out;
    tq.expired(base + ms(5), out);
    check(out.empty(), "아무것도 만기 전");

    tq.expired(base + ms(15), out);
    check(out.size() == 1 && out[0] == &b, "b 먼저 만기");
    out.clear();

    // 재무장: a 를 더 뒤로 밀면 낡은 항목은 무효화된다.
    tq.arm(&a, base + ms(100));
    tq.expired(base + ms(35), out);
    check(out.size() == 1 && out[0] == &c, "c 만기, a 는 재무장돼 아직 아님");
    out.clear();

    // 취소: a 를 취소하면 만기 콜백이 오지 않는다.
    tq.cancel(&a);
    tq.expired(base + ms(200), out);
    check(out.empty(), "취소된 a 는 만기 안 함");
    check(tq.empty(), "모두 소진돼 빈 큐");

    // timeout_ms: 다음 만기까지의 밀리초.
    int d;
    tq.arm(&d, base + ms(50));
    int t = tq.timeout_ms(base + ms(30));
    check(t >= 19 && t <= 21, "timeout_ms ~= 20");
    t = tq.timeout_ms(base + ms(60));
    check(t == 0, "이미 지난 만기는 timeout 0");
    tq.cancel(&d);
    check(tq.timeout_ms(base) == -1, "빈 큐는 timeout -1");
}

// 세대(seq) 재사용 회귀.
//
// 발화·취소가 token 을 map 에서 지우면, 같은 token 으로 다시 arm 할 때 세대가
// 1 부터 다시 시작한다. 힙에 남아 있던 낡은 항목의 세대와 값이 같아지면 그 낡은
// 항목이 "유효" 로 오인되어 (a) 엉뚱한 시점에 조기 만기가 발화하고 (b) 그 발화가
// live_ 를 소진해 진짜 만기는 영영 오지 않는다.
//
// 릴레이에서 이 조건은 드물지 않다: 룸이 게스트 입장 시 데드라인을 재무장하고,
// 연결 상태 객체가 파괴된 자리에 새 연결이 같은 주소로 할당되면 token 이 재사용된다.
void test_timer_generation_reuse() {
    relay::TimerQueue tq;
    auto base = Clock::now();
    int conn;  // 토큰(연결 상태 객체 주소 대용)

    tq.arm(&conn, base + ms(1000));  // 최초 무장 (예: 게스트 대기)
    tq.arm(&conn, base + ms(100));   // 재무장 (예: READY 대기) — 낡은 항목이 남는다

    std::vector<void*> out;
    tq.expired(base + ms(150), out);
    check(out.size() == 1, "재무장된 만기가 발화");
    out.clear();

    // 같은 주소로 새 연결이 들어와 먼 미래로 무장한다.
    tq.arm(&conn, base + ms(2000));

    tq.expired(base + ms(1000), out);
    check(out.empty(), "낡은 항목이 조기 만기를 일으키지 않음");
    out.clear();

    tq.expired(base + ms(2000), out);
    check(out.size() == 1 && out[0] == &conn, "진짜 만기가 유실되지 않음");
}

void test_offload() {
    std::atomic<int> wakes{0};
    relay::Offload off(2, [&] { ++wakes; });

    // 워커에서 블로킹(sleep)한 뒤 continuation 이 루프 스레드에서 실행되는지.
    std::atomic<bool> ran_on_worker{false};
    std::string result_seen;

    off.submit([&]() -> relay::Offload::Cont {
        std::this_thread::sleep_for(ms(20));   // 블로킹 흉내(HTTP 왕복)
        ran_on_worker = true;
        std::string computed = "token-ok";     // 워커가 계산한 결과
        return [&result_seen, computed] { result_seen = computed; };  // 루프에서 실행
    });

    // wake 가 올 때까지 대기(루프가 poll 에서 깨어나는 것에 해당).
    for (int i = 0; i < 200 && wakes.load() == 0; ++i) {
        std::this_thread::sleep_for(ms(5));
    }
    check(wakes.load() >= 1, "완료 시 wake 발화");
    check(ran_on_worker.load(), "job 이 워커에서 실행됨");

    // 아직 continuation 은 실행 전 — drain 하기 전까지는 루프가 손대지 않는다.
    check(result_seen.empty(), "drain 전에는 continuation 미실행");

    std::vector<relay::Offload::Cont> conts;
    off.drain(conts);
    for (auto& c : conts) c();  // 루프 스레드에서 실행
    check(result_seen == "token-ok", "drain 후 continuation 이 결과 반영");

    // 여러 job 동시 제출 → 전부 회수되는지.
    std::atomic<int> done{0};
    for (int i = 0; i < 8; ++i) {
        off.submit([&]() -> relay::Offload::Cont {
            std::this_thread::sleep_for(ms(5));
            return [&done] { ++done; };
        });
    }
    // 폴링으로 8개 continuation 회수.
    int collected = 0;
    for (int i = 0; i < 400 && collected < 8; ++i) {
        std::vector<relay::Offload::Cont> batch;
        off.drain(batch);
        for (auto& c : batch) { c(); ++collected; }
        if (collected < 8) std::this_thread::sleep_for(ms(5));
    }
    check(collected == 8 && done.load() == 8, "8개 job 전부 회수·실행");

    check(off.submit([]() -> relay::Offload::Cont { return {}; }),
          "종료 전 submit 은 수락");

    off.shutdown();  // idempotent — 소멸자도 부르지만 무해

    // 종료 후 제출은 거절돼야 한다. 수락하면 워커가 이미 빠져나갔으므로 그 job 은
    // 영영 실행되지 않고, 호출자는 성공한 줄 알지만 결과가 조용히 사라진다.
    std::atomic<bool> ghost{false};
    bool accepted = off.submit([&]() -> relay::Offload::Cont {
        ghost = true;
        return {};
    });
    check(!accepted, "종료 후 submit 은 거절");
    check(!ghost.load(), "거절된 job 은 실행되지 않음");
}

} // namespace

int main() {
    test_timer_queue();
    test_timer_generation_reuse();
    test_offload();
    if (g_failures == 0) {
        std::fprintf(stderr, "[loop-prim] all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "[loop-prim] %d check(s) failed\n", g_failures);
    return 1;
}
