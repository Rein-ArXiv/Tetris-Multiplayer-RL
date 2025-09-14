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

// [NET] 1:1 P2P Lockstep 최소 세션
// - HELLO/SEED 교환으로 시드/시작틱/input_delay 합의
// - 틱 입력 전송/수신, 안전 틱까지 시뮬레이션 진행
// - 학습용으로 TCP 사용(순서/신뢰 보장)

namespace net {

enum class Role : uint8_t { Host=1, Peer=2 };

struct SeedParams {
    uint64_t seed{0};
    uint32_t start_tick{120};
    uint8_t input_delay{4};
    Role role{Role::Host};
};

class Session {
public:
    Session();
    ~Session();

    // 호스트로 리스닝 시작 후 1명 수락
    bool Host(uint16_t port, const SeedParams& sp);
    // 피어로 접속
    bool Connect(const std::string& host, uint16_t port);

    // 연결/합의 상태
    bool isConnected() const { return connected; }
    bool isReady() const { return ready; }
    SeedParams params() const { return seedParams; }

    // 입력 전송(틱, 비트마스크)
    void SendInput(uint32_t tick, uint8_t mask);

    // 원격 입력 조회: tick의 입력이 준비되어 있으면 true
    bool GetRemoteInput(uint32_t tick, uint8_t& outMask);

    // 마지막으로 수신/송신한 입력 틱(안전 틱 계산에 활용)
    uint32_t maxRemoteTick() const { return lastRemoteTick; }
    uint32_t maxLocalTick() const { return lastLocalTick; }

    // 종료
    void Close();

private:
    void ioThread();
    void handleFrame(const Frame& f);

    TcpSocket sock{};
    std::thread th;
    std::atomic<bool> quit{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> ready{false};

    SeedParams seedParams{};

    // 수신 버퍼/프레이밍
    std::vector<uint8_t> recvBuf;

    // 송신 큐
    std::mutex sendMu;
    std::deque<std::vector<uint8_t>> sendQ;

    // 원격 입력 저장
    std::mutex inMu;
    std::unordered_map<uint32_t, uint8_t> remoteInputs;
    std::atomic<uint32_t> lastRemoteTick{0};
    std::atomic<uint32_t> lastLocalTick{0};
};

}

