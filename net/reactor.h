#pragma once
#include <cstdint>
#include <memory>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// net/reactor.h — 단일 스레드 이벤트 루프(reactor)의 플랫폼 독립 계약
//
// 왜 존재하는가
//   기존 릴레이는 연결(방향)마다 스레드 하나를 두고 "논블로킹 recv + 1ms sleep"
//   으로 폴링했다. 수백 연결까지는 커널이 사실상 이벤트 루프 역할을 대신해 주지만,
//   유휴 스레드마다 주기적으로 깨어나는 busy-poll 비용이 연결 수에 비례해 쌓인다.
//   reactor 는 그 폴링을 커널의 준비성 통지(readiness notification)로 대체한다 —
//   OS 가 "이 소켓 읽을 수 있다"고 알려줄 때만 깨어난다.
//
// 준비성(readiness) 모델을 선택한 이유
//   두 계열의 OS API 가 있다.
//     - 준비성(epoll/kqueue): "읽을 수 있게 됐다" → 내가 recv 한다. Linux/BSD.
//     - 완료(IOCP): 버퍼를 미리 주고 recv 를 걸어두면 "다 읽었다" 고 통지. Windows.
//   이 인터페이스는 준비성으로 통일한다. 순차 recv 로직(net/socket.cpp 재사용)이
//   그대로 살아남기 때문이다. Windows(IOCP)에서는 zero-byte WSARecv 로 read 준비성을
//   에뮬레이션한다 — 0바이트 수신 완료는 "소비하지 않고도 읽을 데이터가 있다"는
//   신호이므로, 통지를 받은 뒤 실제 recv 는 논블로킹으로 즉시 성공한다.
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
struct Event {
    void* token   = nullptr;
    bool  readable = false;  // recv 가능(또는 EOF — recv 가 0/에러를 돌려줘 확인)
    bool  writable = false;  // 보류 송신을 재시도할 수 있음
    bool  error    = false;  // HUP/ERR — 다음 recv/send 에서 확정 처리
};

// 플랫폼별 준비성 이벤트 루프. 한 스레드가 소유한다(인스턴스 자체는 thread-safe
// 아님). 예외는 wake() 하나 — 다른 스레드/시그널 문맥에서 루프를 깨우기 위해서만
// 호출할 수 있어야 하고, 각 백엔드가 그 한정된 thread-safety 를 보장한다.
class Reactor {
public:
    // 플랫폼에 맞는 구현을 만든다(Linux=epoll, Windows=IOCP). 실패 시 nullptr.
    static std::unique_ptr<Reactor> create();

    virtual ~Reactor() = default;

    // fd 를 관심 집합에 등록/변경/해제. token 은 이벤트에 그대로 실려 돌아온다.
    // add 는 이미 등록된 fd 에, remove 는 미등록 fd 에 대해 false 를 돌려줄 수 있다.
    virtual bool add(int fd, unsigned interest, void* token) = 0;
    virtual bool modify(int fd, unsigned interest, void* token) = 0;
    virtual bool remove(int fd) = 0;

    // 최대 timeout_ms 동안 준비된 fd 를 기다린다(음수면 무기한). 준비된 이벤트를
    // out 에 채우고 개수를 돌려준다. 0 = 타임아웃(만기 처리하러 나가라는 뜻),
    // -1 = 회복 불가 오류. out 은 매 호출마다 지워진 뒤 채워진다.
    virtual int poll(std::vector<Event>& out, int timeout_ms) = 0;

    // 등록된 소켓을 다른 Reactor 인스턴스로 옮길 수 있는가.
    //
    // 준비성 모델(epoll)에서는 관심 집합이 커널의 epoll 인스턴스에 있을 뿐이라
    // 한쪽에서 빼고 다른 쪽에 넣으면 그만이다. 완료 모델(IOCP)에서는 불가능하다 —
    // 소켓 핸들은 완료 포트에 결합되면 수명이 끝날 때까지 그 포트에 묶이고 다시
    // 결합할 수 없다. 매치를 다른 루프로 넘기는 샤딩은 이 능력을 전제하므로,
    // 호출자는 여기서 false 를 받으면 단일 루프로 물러서야 한다.
    virtual bool can_migrate_sockets() const = 0;

    // 블로킹 중인 poll() 을 즉시 깨운다. 다른 스레드나 시그널 문맥(종료 플래그를
    // 세운 직후 등)에서 호출해도 안전한 유일한 메서드다. 깨어난 poll() 은 0개
    // 이벤트로 반환될 수 있으므로 호출자는 종료/작업 플래그를 스스로 확인한다.
    virtual void wake() = 0;
};

} // namespace net
