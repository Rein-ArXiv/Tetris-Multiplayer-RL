// server/matchmaker.h — FIFO 매치 큐
//
// 역할: 들어오는 플레이어를 FIFO 로 쌓아두고 2명이 모이면 페어링 해서 Match 리턴.
// 단일 프로듀서(accept 스레드 → playerConnThread) / 단일 컨슈머(matcher 스레드).
// 동기화는 std::mutex + std::condition_variable.
//
// 학습 포인트:
//   - std::condition_variable 의 전형적 predicate wait 패턴
//   - shutdown 시 대기 스레드 깨우기 (notify_all + 상태 플래그)
//   - 소켓 핸들(TcpSocket) 은 shared_ptr<int> 기반 참조 카운트 소유 핸들이다.
//     복사본들이 같은 fd 소유권을 공유하며, 마지막 복사본이 소멸할 때 fd 가
//     정확히 한 번 ::close 된다. 따라서 move/복사 후에도 fd 는 살아있고, 모든
//     복사본이 사라질 때까지 OS 가 그 fd 번호를 새 연결에 재사용하지 않는다.

#pragma once
#include "../net/socket.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace relay {

// 큐에 들어간 플레이어 정보.
// player_id / elo / username / token 은 meta /v1/auth/verify 성공 시 채워진다.
// meta 비활성화(--meta 없음) 또는 토큰 미제공 시 player_id=0 (unranked).
struct PlayerInfo {
    net::TcpSocket sock;
    uint32_t       conn_id{0};  // 로깅용
    int64_t        player_id{0};
    int            elo{0};
    std::string    username;    // empty = guest (no nickname yet)
    std::string    token;       // relay 가 /v1/matches 에 참조 없이 전달은 안 함
    std::string    selected_icon_id{"default"};
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
