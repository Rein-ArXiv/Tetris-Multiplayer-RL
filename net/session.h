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

// 게임 오버 후 선택
enum class GameOverChoice : uint8_t {
    None = 0,
    Restart = 1,
    GoToTitle = 2,
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
                   uint32_t start_tick = 120, uint8_t input_delay = 2);
    // 매칭 대기 중 취소. 소켓을 닫아 큐 스레드를 즉시 해제.
    void QueueCancel();

    // 세션 상태
    bool isConnected() const { return connected; }
    bool isReady() const { return ready; }
    bool isListening() const { return listening; }
    bool hasFailed() const { return connectionFailed; }
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

    // 안전 틱 계산용: safeTick = min(local, remote) - inputDelay
    uint32_t maxRemoteTick() const { return lastRemoteTick; }
    uint32_t maxLocalTick() const { return lastLocalTick; }

    void ClearInputs();  // 재시작 시 입력 큐 초기화
    void Close();  // 세션 종료 (스레드 정리, 소켓 닫기)

private:
    void ioThread();  // I/O 루프 (송수신, 메시지 파싱)
    void handleFrame(const Frame& f);  // 메시지 처리
    void acceptThread(uint16_t port);  // 호스트 전용: 연결 대기
    void queueThread(std::string host, uint16_t port,
                     uint32_t start_tick, uint8_t input_delay);  // 릴레이 큐잉 전용

    TcpSocket sock{};
    TcpSocket listenSock{};

    std::thread th;
    std::thread ath;
    std::thread qth;

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

    std::atomic<uint32_t> lastHashTickRemote{0};
    std::atomic<uint64_t> lastHashRemote{0};

    std::atomic<uint8_t> localGameOverChoice{0};
    std::atomic<uint8_t> remoteGameOverChoice{0};
};

}
