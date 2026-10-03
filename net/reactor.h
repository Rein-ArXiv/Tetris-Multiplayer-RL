#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "native_socket.h"

// ─────────────────────────────────────────────────────────────────────────────
// net/reactor.h — 단일 스레드 이벤트 루프(reactor)의 플랫폼 독립 계약
//
// 왜 존재하는가
//   기존 릴레이는 연결(방향)마다 스레드 하나를 두고 "논블로킹 recv + 1ms sleep"
//   으로 폴링했다. 이 대기의 비용은 연결 수·활동 빈도·스케줄링 조건에 따라
//   달라지므로 유휴 CPU와 입력 지연을 실제 부하에서 함께 측정해야 한다.
//   reactor 는 주기적인 수신 확인을 커널의 준비성 통지로 대체한다. poll 은
//   I/O뿐 아니라 시간 초과·wake·오류로도 반환하며, 실제 재실행 시각에는 OS의
//   스케줄링 지연이 더해질 수 있다.
//
// 준비성(readiness) 모델을 선택한 이유
//   두 계열의 OS API 가 있다.
//     - 준비성(epoll/kqueue): "읽을 수 있게 됐다" → 내가 recv 한다. Linux/BSD.
//     - 완료(IOCP): 버퍼와 recv 연산을 제출하고 완료 시 수량·상태를 확인. Windows.
//   이 인터페이스는 준비성으로 통일한다. 순차 recv 로직(net/socket.cpp 재사용)이
//   그대로 살아남기 때문이다. Windows(IOCP)에서는 zero-byte WSARecv 로 read 준비성을
//   에뮬레이션한다. 0바이트 완료는 읽기 힌트로 변환하되, 양수 길이의 실제
//   논블로킹 recv 결과로 데이터·EOF·오류·WouldBlock을 판정한다.
//
// 무엇을 하지 않는가
//   reactor 는 I/O 준비성만 다룬다. 타임아웃(방향별 idle, 룸 데드라인)은 루프 상위가
//   다음 만기까지의 시간을 계산해 poll() 의 timeout_ms 로 넘긴다. 블로킹 호출(meta
//   HTTP POST, DNS)은 절대 루프 안에서 부르지 않는다 — 한 핸들러가 막히면 그 매치만이
//   아니라 전원이 멈추기 때문이다. 그런 일은 워커로 오프로드하고 결과만 wake() 로
//   루프에 되돌린다.
// ─────────────────────────────────────────────────────────────────────────────

namespace net {

// 관심 이벤트 비트마스크. Read 는 "읽을 수 있는가", Write 는 "논블로킹 send 가
// 즉시 진행되는가"(보류 송신 버퍼를 비울 때만 켠다).
enum Interest : unsigned {
    kNone  = 0,
    kRead  = 1u << 0,
    kWrite = 1u << 1,
};

// 한 번의 poll 이 돌려주는 이벤트. token 은 add() 때 등록한 불투명 포인터로,
// 보통 연결 상태 객체를 가리킨다. reactor 는 token 을 해석하지 않는다.
// 등록 제거는 이미 반환한 Event를 지우지 않는다. 호출자가 그 참조의 수명을 지킨다.
struct Event {
    void* token   = nullptr;
    bool  readable = false;  // recv 가능(또는 EOF — recv 가 0/에러를 돌려줘 확인)
    bool  writable = false;  // 보류 송신을 재시도할 수 있음
    bool  error    = false;  // HUP/ERR — 다음 recv/send 에서 확정 처리
};

// 플랫폼별 준비성 이벤트 루프. 한 스레드가 소유한다(인스턴스 자체는 thread-safe
// 아님). wake()만 다른 일반 스레드에서 호출할 수 있다. 파괴 전에 모든 wake
// 호출자를 중단·회수해야 한다. 공통 인터페이스는 async-signal-safety를 보장하지 않는다.
class Reactor {
public:
    // 플랫폼에 맞는 구현을 만든다(Linux=epoll, Windows=IOCP). 실패 시 nullptr.
    static std::unique_ptr<Reactor> create();

    virtual ~Reactor() = default;

    // fd 를 관심 집합에 등록/변경/해제. token 은 이벤트에 그대로 실려 돌아온다.
    // add 는 이미 등록된 fd 에, remove 는 미등록 fd 에 대해 false 를 돌려줄 수 있다.
    virtual bool add(NativeSocket fd, unsigned interest, void* token) = 0;
    virtual bool modify(NativeSocket fd, unsigned interest, void* token) = 0;
    virtual bool remove(NativeSocket fd) = 0;

    // timeout_ms를 요청 대기 시간으로 사용한다(음수면 무기한). 스케줄링 지연으로
    // 실제 경과는 더 길 수 있다. out을 비운 뒤 연결 이벤트 수를 반환한다.
    // 0은 timeout뿐 아니라 wake/중단일 수 있다. -1은 대기 오류다.
    // 호출자는 매번 실제 시각·작업 큐·종료 조건을 다시 확인한다.
    virtual int poll(std::vector<Event>& out, int timeout_ms) = 0;

    // 등록된 소켓을 다른 Reactor 인스턴스로 옮길 수 있는가.
    //
    // 준비성 모델(epoll)에서는 관심 집합이 커널의 epoll 인스턴스에 있을 뿐이라
    // 이전 이벤트 참조와 소유권을 정리하고 다시 등록할 수 있다. IOCP에서는 불가능하다 —
    // 소켓 핸들은 완료 포트에 결합되면 수명이 끝날 때까지 그 포트에 묶이고 다시
    // 결합할 수 없다. 매치를 다른 루프로 넘기는 샤딩은 이 능력을 전제하므로,
    // 호출자는 여기서 false 를 받으면 단일 루프로 물러서야 한다.
    virtual bool can_migrate_sockets() const = 0;

    // 다른 일반 스레드가 poll()의 대기를 깨우도록 요청한다. 작업은 먼저 동기화된
    // 큐/플래그에 공개한다. 통지는 합쳐질 수 있으며 정확한 실행 시각을 보장하지 않는다.
    // 0개 연결 이벤트로 반환될 수 있으므로 호출자는 작업·종료 조건을 다시 확인한다.
    virtual void wake() = 0;
};

} // namespace net
