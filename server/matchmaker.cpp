#include "matchmaker.h"
#include "match_uuid.h"

#include "../net/framing.h"
#include "log.h"

#include <chrono>
#include <utility>
#include <vector>

namespace relay {

namespace {

bool waitingPlayerStillActive(PlayerInfo& p) {
    // p.streamBuf 에 누적 수신 — 로컬 버퍼를 쓰면 폴링 사이에 걸친 부분 프레임
    // 바이트가 유실되어 스트림이 어긋난다. parse_frames 가 완성 프레임만큼만
    // 소비하고 잔여 tail 은 다음 폴링/로비 단계로 넘어간다.
    if (!net::tcp_recv_some(p.sock, p.streamBuf)) {
        RLOG_INFO("[matchmaker] conn=" << p.conn_id
                  << " player_id=" << p.player_id
                  << " left queue before match");
        net::tcp_close(p.sock);
        return false;
    }

    if (!p.streamBuf.empty()) {
        std::vector<net::Frame> frames;
        if (!net::parse_frames(p.streamBuf, frames)) {
            RLOG_INFO("[matchmaker] conn=" << p.conn_id
                      << " player_id=" << p.player_id
                      << " sent malformed queue frame");
            net::tcp_close(p.sock);
            return false;
        }
        for (const auto& f : frames) {
            if (f.type == net::MsgType::QUEUE_CANCEL) {
                RLOG_INFO("[matchmaker] conn=" << p.conn_id
                          << " player_id=" << p.player_id
                          << " cancelled queue");
                net::tcp_close(p.sock);
                return false;
            }
        }
    }

    return true;
}

}  // namespace

Matchmaker::Matchmaker() = default;

Matchmaker::~Matchmaker() {
    shutdown();
}

// 매치 seed 는 MATCH_FOUND 로 두 클라이언트에게 그대로 나간다. 스트림에서 뽑으면
// 받은 값이 곧 생성기 상태가 되어 이후 매치가 전부 예측된다 — match_seed.h 참조.
// 분배 품질이 중요한 RL 시뮬레이션 쪽은 SimGame 이 자체 RNG 를 가지고 있음.
uint64_t Matchmaker::nextSeed() {
    return seed_src.next();
}

bool Matchmaker::enqueue(PlayerInfo p) {
    {
        std::lock_guard<std::mutex> lk(mu);
        // Serialize admission with shutdown: a late producer cannot repopulate
        // a queue whose consumer has already stopped.
        if (stopping.load() || waiting.size() >= kMaxWaiting) {
            net::tcp_close(p.sock);
            return false;
        }
        waiting.push_back(std::move(p));
        cv.notify_one();
    }
    return true;
}

std::optional<Match> Matchmaker::waitForPair() {
    std::unique_lock<std::mutex> lk(mu);
    while (true) {
        if (stopping.load()) return std::nullopt;
        // Observe cancellation even when only one player is waiting. Poll all
        // pending entries so stale sessions do not retain admission leases.
        for (auto it = waiting.begin(); it != waiting.end();) {
            if (!waitingPlayerStillActive(*it)) it = waiting.erase(it);
            else ++it;
        }
        if (waiting.size() >= 2) break;
        if (waiting.empty()) {
            cv.wait(lk, [this] { return stopping.load() || !waiting.empty(); });
        } else {
            // Socket data does not notify this condition_variable.
            // This is a cooperative polling interval, not a hard deadline.
            cv.wait_for(lk, std::chrono::milliseconds(50));
        }
    }

    Match m;
    m.a = std::move(waiting.front()); waiting.pop_front();
    m.b = std::move(waiting.front()); waiting.pop_front();
    m.seed = nextSeed();
    m.match_id = next_match_id++;
    m.match_uuid = new_match_uuid();
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
