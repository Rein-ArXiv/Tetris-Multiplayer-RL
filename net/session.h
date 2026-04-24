#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <string>
#include "socket.h"
#include "framing.h"

// P2P Lockstep 세션: 결정론적 멀티플레이어 (모든 입력 도착 시까지 시뮬레이션 대기)

namespace net {

// 네트워크 역할
enum class Role : uint8_t { Host=1, Peer=2 };

// 링크 건강 상태 — 마지막 PONG 수신 경과 시간 기반
//   OK     : 마지막 PONG < 2s (정상)
//   Stalled: 2s ≤ 경과 < 10s (상대가 잠시 얼어붙음, Windows 창 드래그 등)
//   Lost   : 경과 ≥ 10s 혹은 hasFailed() — 연결 공식 단절로 간주
enum class LinkStatus : uint8_t { OK=0, Stalled=1, Lost=2 };

// 게임 오버 후 선택
enum class GameOverChoice : uint8_t {
    None = 0,
    Restart = 1,
    GoToTitle = 2,
};

// 커스텀 룸 상태 머신 (로비 대기 페이즈).
// MATCH_FOUND 수신 후에는 Session 이 일반 게임 세션으로 전환되므로
// 이 상태는 더 이상 의미 없다 (isReady() 사용).
enum class RoomState : uint8_t {
    Idle            = 0,
    Connecting      = 1,   // tcp_connect + ROOM_CREATE/ROOM_JOIN 송신 중
    Waiting         = 2,   // 방에 나 혼자 (peer_count=1)
    WaitingWithPeer = 3,   // 두 명 입장 (peer_count=2), READY 대기
    NotFound        = 4,   // 코드에 해당하는 방 없음
    Full            = 5,   // 방이 이미 2명이어서 입장 불가
    GoneFull        = 6,   // 상대가 퇴장해 다시 혼자 (서버가 ROOM_INFO(3) 푸시)
    Failed          = 7,   // 소켓 오류 / 연결 실패
    Starting        = 8,   // MATCH_FOUND 수신, 곧 게임 시작
};

// 게임 시작 파라미터 (호스트가 결정 → SEED 메시지로 전달)
struct SeedParams {
    uint64_t seed{0};
    uint32_t start_tick{120};
    uint8_t input_delay{2};
    Role role{Role::Host};
};

class Session {
public:
    Session();
    ~Session();

    // 네트워크 연결
    bool Host(uint16_t port, const SeedParams& sp);  // 호스트: 포트 대기, 파라미터 결정
    bool Connect(const std::string& host, uint16_t port);  // 클라이언트: 호스트 연결

    // 릴레이 서버가 페어링한 후 MATCH_FOUND 로 받은 정보를 주입.
    // - 이미 연결된 소켓을 그대로 채택 (HELLO/SEED 핸드셰이크 생략)
    // - role/seed 는 릴레이가 결정해 보내줌
    // - ready=true 즉시, 게임 시작 가능
    bool Adopt(TcpSocket socket, Role role, uint64_t seed,
               uint32_t start_tick = 120, uint8_t input_delay = 2);

    // 비동기 릴레이 큐잉 — 메인 스레드를 블록하지 않는다.
    //   1) tcp_connect + QUEUE_JOIN 송신
    //   2) MATCH_FOUND 대기 (최대 5분, 내부 큐 스레드)
    //   3) 수신 시 seedParams 채우고 ioThread 시작 → ready=true
    // 호출 즉시 true 리턴 (큐 스레드가 기동된 경우). 연결 자체가 실패하면 false.
    // 호출부는 isReady() / hasFailed() 로 진행 상태를 폴링한다.
    bool QueueJoin(const std::string& host, uint16_t port,
                   uint32_t start_tick = 120, uint8_t input_delay = 2,
                   const std::string& auth_token = {});
    // 매칭 대기 중 취소. 소켓을 닫아 큐 스레드를 즉시 해제.
    void QueueCancel();

    // 랜덤 큐 수락 로비 (MATCH_FOUND 수신 이후 ~ 게임 시작 직전).
    //   · isQueueMatched()   : 서버가 상대를 페어링해 MATCH_FOUND 를 보냈지만
    //                          아직 양쪽 READY(1) 수락은 끝나지 않은 상태.
    //   · queueLocalReady()  : 내가 QueueConfirm(true) 을 보냈는가.
    //   · queuePeerReady()   : 상대도 READY(1) 을 보냈는가 (릴레이가 forward).
    //   · QueueConfirm()     : 로비에서 "수락" — READY(1) 전송.
    //   · QueueDecline()     : 로비에서 "거절" — READY(0) 전송 후 연결 종료.
    // 양쪽 ready 가 되면 queueThread 가 자동으로 ioThread 로 전환 (ready=true).
    bool isQueueMatched() const { return queueMatched_.load(); }
    bool queueLocalReady() const { return queueLocalReady_.load(); }
    bool queuePeerReady() const { return queuePeerReady_.load(); }
    void QueueConfirm();
    void QueueDecline();

    // 커스텀 룸 경로 — QueueJoin 과 유사한 비동기 구조.
    //   RoomCreate : 서버가 5자리 코드 발급 후 ROOM_INFO 회신
    //   RoomJoin   : 기존 코드로 입장
    // 두 메서드 모두 즉시 true 를 리턴하고, 진행 상태는 roomState() 로 폴링.
    // MATCH_FOUND 도착 시 QueueJoin 과 동일하게 ioThread 기동 + ready=true.
    bool RoomCreate(const std::string& host, uint16_t port,
                    uint32_t start_tick = 120, uint8_t input_delay = 2,
                    const std::string& auth_token = {});
    bool RoomJoin(const std::string& host, uint16_t port,
                  const std::string& code,
                  uint32_t start_tick = 120, uint8_t input_delay = 2,
                  const std::string& auth_token = {});
    // READY 플래그 송신 (양쪽 true 시 서버가 MATCH_FOUND 발행).
    void RoomSendReady(bool ready);
    // ROOM_LEAVE 송신 후 소켓 종료 — 큰 방을 떠난다.
    void RoomLeave();

    RoomState   roomState() const { return roomState_.load(); }
    int         roomPeerCount() const { return roomPeerCount_.load(); }
    std::string roomCode() const {
        std::lock_guard<std::mutex> lk(roomMu_);
        return roomCode_;
    }

    // 세션 상태
    bool isConnected() const { return connected; }
    bool isReady() const { return ready; }
    bool isListening() const { return listening; }
    bool hasFailed() const { return connectionFailed; }
    // PING/PONG 기반 링크 건강 상태 — ready=true 이후 유효.
    LinkStatus linkStatus() const;
    SeedParams params() const {
        std::lock_guard<std::mutex> lk(seedMu);
        return seedParams;
    }

    // 게임 데이터 송신
    void SendInput(uint32_t tick, uint8_t mask);
    void SendHash(uint32_t tick, uint64_t hash);
    void SendGameOverChoice(GameOverChoice choice);
    void SendNewSeed(uint64_t newSeed);

    // 게임 데이터 수신
    bool GetRemoteInput(uint32_t tick, uint8_t& outMask);
    bool GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const;
    bool GetRemoteGameOverChoice(GameOverChoice& outChoice) const;
    void ClearGameOverChoices();

    // 채팅 (Section E) — CHAT 프레임은 릴레이를 투명 통과.
    //   SendChat: UTF-8 텍스트를 프레임 1개에 담아 송신. 최대 200자 가정(상한은 MAX_PAYLOAD_BYTES).
    //   PullChat: 수신 큐에서 한 줄 꺼냄. 없으면 false.
    void SendChat(const std::string& text);
    bool PullChat(std::string& outText);

    // Section K — 랭킹 연동 (tetris_meta).
    //   SendMatchSummary: 게임오버 진입 시 1회 송신. won + 자기/상대 관측치.
    //   GetMatchResult:   relay 가 POST /v1/matches 결과를 돌려주면 채워짐.
    //                     delta=0 은 "ranking offline" 시그널. 한번 돌아온 결과는
    //                     호출 시마다 true 반환 (누적 플래그 아님 — Session 재사용 시
    //                     ClearGameOverChoices 에서 함께 리셋).
    void SendMatchSummary(uint8_t won,
                          uint32_t my_score, uint32_t my_lines,
                          uint32_t opp_score, uint32_t opp_lines,
                          uint32_t duration_s);
    struct MatchResult { int32_t elo_before; int32_t elo_after; int32_t delta; };
    bool GetMatchResult(MatchResult& out) const;

    // 안전 틱 계산용: safeTick = min(local, remote) - inputDelay
    uint32_t maxRemoteTick() const { return lastRemoteTick; }
    uint32_t maxLocalTick() const { return lastLocalTick; }

    // ioThread 가 메인 스레드 스톨(창 드래그 등) 을 감지해 대신 송신한 INPUT(tick,0) 의
    // 최대 틱. 메인 스레드가 깨어나면 자기 localTickNext 를 이 값 + 1 로 당겨
    // 같은 tick 들은 0 으로 채워야 한다 (peer 쪽은 이미 0 으로 받았기 때문).
    //   0 이면 heartbeat 이 한 번도 발동 안 함 — 메인이 정상 주행 중.
    uint32_t heartbeatTickEnd() const { return heartbeatTickEnd_.load(); }

    void ClearInputs();  // 재시작 시 입력 큐 초기화
    void Close();  // 세션 종료 (스레드 정리, 소켓 닫기)

private:
    void ioThread();  // I/O 루프 (송수신, 메시지 파싱)
    void handleFrame(const Frame& f);  // 메시지 처리
    void acceptThread(uint16_t port);  // 호스트 전용: 연결 대기
    void queueThread(std::string host, uint16_t port,
                     uint32_t start_tick, uint8_t input_delay,
                     std::string auth_token);  // 릴레이 큐잉 전용
    // 커스텀 룸 대기 스레드: ROOM_CREATE 또는 ROOM_JOIN 전송 후 ROOM_INFO/MATCH_FOUND 수신.
    // joinCode 가 비어 있으면 CREATE, 아니면 JOIN.
    void roomThread(std::string host, uint16_t port,
                    std::string joinCode,
                    uint32_t start_tick, uint8_t input_delay,
                    std::string auth_token);

    TcpSocket sock{};
    TcpSocket listenSock{};

    std::thread th;
    std::thread ath;
    std::thread qth;
    std::thread rth;  // 룸 대기 스레드

    // 룸 상태 (roomThread 만 기록, 메인 스레드만 읽음)
    std::atomic<RoomState> roomState_{RoomState::Idle};
    std::atomic<int>       roomPeerCount_{0};
    mutable std::mutex     roomMu_;
    std::string            roomCode_;  // 서버가 준 코드 (CREATE) 또는 시도한 코드 (JOIN)
    // 룸 스레드로 전달될 아웃바운드 프레임 큐 (READY/ROOM_LEAVE 등).
    std::mutex                        roomSendMu_;
    std::deque<std::vector<uint8_t>>  roomSendQ_;

    // 랜덤 큐 로비 단계 sock 쓰기 직렬화. queueThread drain 과 main thread 의
    // QueueDecline 동기 송신이 같은 fd 에 interleaved tcp_send_all 을 걸지 않도록.
    std::mutex                        queueSockSendMu_;

    // 랜덤 큐 수락 로비 상태 (queueThread 전용).
    //   queueMatched_      : MATCH_FOUND 수신 직후 true. ready=true 로 전환되는 순간 false 로 돌린다.
    //   queueLocalReady_   : 메인 스레드가 QueueConfirm() 호출 → queueThread 가 릴레이로 READY(1) 송신 완료.
    //   queuePeerReady_    : 상대가 READY(1) 보내 릴레이가 포워딩.
    std::atomic<bool> queueMatched_{false};
    std::atomic<bool> queueLocalReady_{false};
    std::atomic<bool> queuePeerReady_{false};
    // 로비 단계에서 queueThread 로 전달할 outbound 프레임 (READY/QUEUE_CANCEL).
    std::mutex                        queueSendMu_;
    std::deque<std::vector<uint8_t>>  queueSendQ_;

    std::atomic<bool> quit{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> ready{false};
    std::atomic<bool> listening{false};
    std::atomic<bool> connectionFailed{false};

    mutable std::mutex seedMu;
    SeedParams seedParams{};
    std::vector<uint8_t> recvBuf;

    std::mutex sendMu;
    std::deque<std::vector<uint8_t>> sendQ;

    std::mutex inMu;
    std::unordered_map<uint32_t, uint8_t> remoteInputs;
    std::atomic<uint32_t> lastRemoteTick{0};
    std::atomic<uint32_t> lastLocalTick{0};

    // 주의: tick 과 hash 는 pair 로 원자 갱신되어야 한다. 두 atomic 을 쪼개서
    // 쓰면 store 사이에 reader 가 들어가 새 tick + 옛 hash 를 읽어 DESYNC 오탐.
    // 단일 mutex 로 pair 전체를 보호. HASH 프레임은 10s 주기라 lock 부담 없음.
    mutable std::mutex hashMu_;
    uint32_t lastHashTickRemote{0};
    uint64_t lastHashRemote{0};

    std::atomic<uint8_t> localGameOverChoice{0};
    std::atomic<uint8_t> remoteGameOverChoice{0};

    // PING/PONG 하트비트 — steady_clock milliseconds.
    // lastPongMs 는 ready=true 전환 시점에 now 로 초기화.
    std::atomic<int64_t> lastPongMs{0};
    std::atomic<int64_t> lastPingSentMs{0};

    // 메인 스레드 스톨 감지 — 창 드래그 시 WM_ENTERSIZEMOVE 모달 루프가 메인을
    // 점유해 SendInput 이 멈춘다. ioThread 는 별개 스레드라 계속 살아있으므로
    // 스톨 구간 동안 대신 INPUT(tick,0) 을 흘려 lockstep 을 진행시킨다.
    //   lastMainActivityMs_  : SendInput 호출 시 now_ms(). 0 = 첫 입력 이전.
    //   heartbeatTickEnd_    : auto-heartbeat 으로 송신된 마지막 tick. 0 = 비활성.
    //   lastHeartbeatMs_     : heartbeat rate limit (16ms = 60Hz).
    std::atomic<int64_t>  lastMainActivityMs_{0};
    std::atomic<uint32_t> heartbeatTickEnd_{0};
    int64_t               lastHeartbeatMs_{0};  // ioThread 전용

    // 채팅 수신 큐 — io 스레드가 채우고 메인 스레드가 PullChat 으로 비움.
    std::mutex               chatMu_;
    std::deque<std::string>  chatQ_;

    // Section K — MATCH_RESULT 수신 (단일 슬롯, 재대입 가능).
    mutable std::mutex   matchResultMu_;
    bool                 matchResultValid_ = false;
    MatchResult          matchResult_{};
};

}
