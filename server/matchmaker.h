// server/matchmaker.h — FIFO 매치 큐
//
// 역할: 들어오는 플레이어를 FIFO 로 쌓아두고 2명이 모이면 페어링 해서 Match 리턴.
// 단일 프로듀서(accept 스레드 → playerConnThread) / 단일 컨슈머(matcher 스레드).
// 동기화는 std::mutex + std::condition_variable.
//
// 학습 포인트:
//   - std::condition_variable 의 전형적 predicate wait 패턴
//   - shutdown 시 대기 스레드 깨우기 (notify_all + 상태 플래그)
//   - 소켓 핸들(TcpSocket = { int fd }) 은 trivially copyable 이므로
//     move 후에도 원본이 살아있음. 의도적으로 "마지막 소유자" 규칙으로 관리.

#pragma once
#include "../net/socket.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

namespace relay {

// 큐에 들어간 플레이어 정보
struct PlayerInfo {
    net::TcpSocket sock;
    uint32_t       conn_id{0};  // 로깅용
};

// 매칭 결과 (2 명)
//   a = HOST  (먼저 큐 진입)
//   b = GUEST (나중 큐 진입)
struct Match {
    PlayerInfo a;
    PlayerInfo b;
    uint64_t   seed{0};     // 서버가 부여한 결정론적 seed
    uint32_t   match_id{0}; // 로깅용 단조 증가 번호
};

class Matchmaker {
public:
    Matchmaker();
    ~Matchmaker();

    // 프로듀서: QUEUE_JOIN 이 확인된 플레이어를 큐에 등록. 컨슈머를 깨움.
    void enqueue(PlayerInfo p);

    // 컨슈머: 2명 모일 때까지 블로킹. shutdown() 호출 시 std::nullopt.
    std::optional<Match> waitForPair();

    // 모든 대기 스레드를 깨우고 큐에 남은 소켓을 닫는다.
    void shutdown();

private:
    uint64_t nextSeed();  // xorshift64 — 서버 내부 RNG

    std::mutex              mu;
    std::condition_variable cv;
    std::deque<PlayerInfo>  waiting;
    std::atomic<bool>       stopping{false};
    uint32_t                next_match_id{1};
    uint64_t                seed_state{0};
};

}  // namespace relay
