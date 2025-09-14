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

// [NET] 1:1 P2P Lockstep 세션(학습용 최소 구현)
// 목적
// - 시작 파라미터 합의(시드/시작틱/입력지연/역할)와 틱별 입력 교환을 담당합니다.
// - TCP 스트림 위에서 framing을 통해 메시지를 주고받습니다.
// - 원격 입력은 틱→입력 비트마스크 맵에 적재되어, 메인 쓰레드가 안전하게 조회합니다.
// 특징
// - 순서/신뢰는 TCP에 의존합니다.
// - 비동기 I/O 스레드가 수신/송신 큐를 처리하고, 메인 루프는 틱 진행에 집중합니다.

namespace net {

enum class Role : uint8_t { Host=1, Peer=2 };

// [NET] 시작 파라미터(세션 합의 결과)
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

    // [NET] 호스트 역할: 포트에서 1인 수락 후 HELLO/SEED 송신
    bool Host(uint16_t port, const SeedParams& sp);
    // [NET] 피어 역할: 원격에 접속 후 HELLO 송신, SEED 수신 대기
    bool Connect(const std::string& host, uint16_t port);

    // [NET] 연결/합의 상태 쿼리(메인 루프의 진행 조건)
    bool isConnected() const { return connected; }
    bool isReady() const { return ready; }
    SeedParams params() const { return seedParams; }

    // [NET] 입력 전송(틱, 비트마스크). framing으로 직렬화되어 TCP로 송신됩니다.
    void SendInput(uint32_t tick, uint8_t mask);
    // [NET] 상태 해시 전송(학습용 디버그)
    void SendHash(uint32_t tick, uint64_t hash);

    // [NET] 원격 입력 조회. tick에 해당하는 입력이 준비되어 있으면 true를 반환.
    bool GetRemoteInput(uint32_t tick, uint8_t& outMask);
    // [NET] 마지막으로 수신한 원격 해시
    bool GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const;

    // [NET] 마지막으로 수신/송신한 입력 틱(안전 틱 계산에 활용)
    uint32_t maxRemoteTick() const { return lastRemoteTick; }
    uint32_t maxLocalTick() const { return lastLocalTick; }

    // 종료
    void Close();

private:
    void ioThread();                    // [NET] 수신/프레이밍/송신 큐를 처리하는 I/O 스레드
    void handleFrame(const Frame& f);   // [NET] 수신 프레임을 해석하여 내부 상태를 갱신

    TcpSocket sock{};
    std::thread th;
    std::atomic<bool> quit{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> ready{false};

    SeedParams seedParams{};

    // [NET] 수신 누적 버퍼/프레이밍 파서
    std::vector<uint8_t> recvBuf;

    // [NET] 송신 큐(프레임 직렬화 결과를 모았다가 소켓으로 보냄)
    std::mutex sendMu;
    std::deque<std::vector<uint8_t>> sendQ;

    // [NET] 원격 입력 저장: tick -> mask
    std::mutex inMu;
    std::unordered_map<uint32_t, uint8_t> remoteInputs;
    std::atomic<uint32_t> lastRemoteTick{0};
    std::atomic<uint32_t> lastLocalTick{0};
    // [NET] 상태 해시(학습용 표시)
    std::atomic<uint32_t> lastHashTickRemote{0};
    std::atomic<uint64_t> lastHashRemote{0};
};

}
