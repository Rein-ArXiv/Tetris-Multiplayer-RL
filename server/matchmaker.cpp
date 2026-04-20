#include "matchmaker.h"

#include "../net/framing.h"

#include <chrono>
#include <iostream>
#include <utility>
#include <vector>

namespace relay {

namespace {

bool waitingPlayerStillActive(PlayerInfo& p) {
    std::vector<uint8_t> stream;
    if (!net::tcp_recv_some(p.sock, stream)) {
        std::cerr << "[matchmaker] conn=" << p.conn_id
                  << " left queue before match\n";
        net::tcp_close(p.sock);
        return false;
    }

    if (!stream.empty()) {
        std::vector<net::Frame> frames;
        if (!net::parse_frames(stream, frames)) {
            std::cerr << "[matchmaker] conn=" << p.conn_id
                      << " sent malformed queue frame\n";
            net::tcp_close(p.sock);
            return false;
        }
        for (const auto& f : frames) {
            if (f.type == net::MsgType::QUEUE_CANCEL) {
                std::cerr << "[matchmaker] conn=" << p.conn_id
                          << " cancelled queue\n";
                net::tcp_close(p.sock);
                return false;
            }
        }
    }

    return true;
}

}  // namespace

Matchmaker::Matchmaker() {
    // 서버 부팅 시각 기반 초기 seed. 재시작마다 다른 게임이 나오도록.
    using clock = std::chrono::high_resolution_clock;
    seed_state = static_cast<uint64_t>(clock::now().time_since_epoch().count());
    if (seed_state == 0) seed_state = 0xDEADBEEFCAFEBABEULL;
}

Matchmaker::~Matchmaker() {
    shutdown();
}

// xorshift64: 단순하고 빠른 PRNG. 매치마다 새 seed 만 필요하므로 충분.
// 분배 품질이 중요한 RL 시뮬레이션 쪽은 SimGame 이 자체 RNG 를 가지고 있음.
uint64_t Matchmaker::nextSeed() {
    uint64_t x = seed_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    seed_state = x;
    return x;
}

void Matchmaker::enqueue(PlayerInfo p) {
    {
        std::lock_guard<std::mutex> lk(mu);
        waiting.push_back(std::move(p));
    }
    cv.notify_one();
}

std::optional<Match> Matchmaker::waitForPair() {
    std::unique_lock<std::mutex> lk(mu);
    while (true) {
        // predicate 형태의 wait: spurious wakeup 에 안전
        cv.wait(lk, [this] { return stopping.load() || waiting.size() >= 2; });
        if (stopping.load()) return std::nullopt;

        while (!waiting.empty() && !waitingPlayerStillActive(waiting.front())) {
            waiting.pop_front();
        }
        if (waiting.size() < 2) continue;

        while (waiting.size() >= 2 && !waitingPlayerStillActive(waiting[1])) {
            waiting.erase(waiting.begin() + 1);
        }
        if (waiting.size() >= 2) break;
    }

    Match m;
    m.a = std::move(waiting.front()); waiting.pop_front();
    m.b = std::move(waiting.front()); waiting.pop_front();
    m.seed = nextSeed();
    m.match_id = next_match_id++;
    return m;
}

void Matchmaker::shutdown() {
    {
        std::lock_guard<std::mutex> lk(mu);
        if (stopping.exchange(true)) return;  // 이미 셧다운됨
        // 큐에 남은 연결들 닫기 (대기하던 플레이어에게 친절한 종료)
        for (auto& p : waiting) {
            net::tcp_close(p.sock);
        }
        waiting.clear();
    }
    cv.notify_all();
}

}  // namespace relay
