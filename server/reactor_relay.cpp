// server/reactor_relay.cpp — 단일 reactor 루프로 도는 릴레이 (실험용 바이너리)
//
// 스레드 모델(tetris_relay)과 같은 wire 프로토콜을 구현하되, 연결마다 스레드를 두는
// 대신 한 스레드가 net::Reactor 로 모든 소켓의 준비성을 기다린다. 스레드 모델에서
// 각 루프가 하던 일은 여기서 "연결 상태 객체 + 이벤트 핸들러"가 된다.
//
//   accept 루프            → listen fd 의 readable 핸들러
//   playerConnThread       → Stage::FirstFrame 핸들러 (+ 인증은 오프로드)
//   matcher 스레드          → 큐에 두 명이 차는 순간 루프가 직접 페어링
//   queueLobbyThread       → Stage::Lobby 핸들러
//   forwarderLoop × 2      → 양쪽 Conn 의 readable 핸들러 하나씩
//
// 단일 스레드가 모든 소켓을 소유하므로 스레드 모델의 동기화가 대부분 사라진다 —
// 방향별 send mutex, 요약 수집 mutex, forwarder_count/disconnect_side atomic,
// 그리고 "양방향이 동시에 죽을 때 교차검증이 생략되는" 경합까지. 남은 락은 오프로드
// 워커와 공유하는 지점(인증 캐시, 세션 lease)뿐이다.
//
// 이번 단계의 범위: 큐 경로(QUEUE_JOIN) 전체. 룸 경로(ROOM_CREATE/ROOM_JOIN)는
// 아직 이관하지 않아 거절한다 — 그 경로가 필요하면 스레드 모델 바이너리를 쓴다.

#include "../net/framing.h"
#include "../net/reactor.h"
#include "../net/socket.h"
#include "../meta/http_client.h"
#include "match_uuid.h"
#include "offload.h"
#include "player_session.h"
#include "timer_queue.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace relay {
namespace {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// 스레드 모델과 같은 값을 쓴다 — 두 바이너리의 동작이 갈리지 않게.
constexpr auto   kFirstFrameTimeout = std::chrono::seconds(5);
constexpr auto   kLobbyTimeout      = std::chrono::seconds(30);
constexpr auto   kIdleTimeout       = std::chrono::seconds(15);
constexpr size_t kMaxBytesPerSecond = 64 * 1024;
constexpr size_t kMaxLobbyBufBytes  = 64 * 1024;
constexpr size_t kMaxHandshakesPerIp = 16;
constexpr size_t kMaxConns          = 512;
// 보류 송신이 이만큼 쌓이면 그 소켓으로 흘려보내는 쪽의 읽기를 멈춘다(backpressure).
// 스레드 모델은 tcp_send_all 이 최대 5초 잠들며 버텼지만, 루프는 잠들 수 없으므로
// 상대가 안 읽으면 읽기를 멈춰 메모리를 지킨다.
constexpr size_t kSendHighWater     = 256 * 1024;

std::atomic<bool> g_running{true};

void on_signal(int) { g_running.store(false); }

// ── MATCH_SUMMARY (고정 21바이트) ────────────────────────────────────────────
struct Summary {
    uint8_t  won;
    uint32_t my_score, my_lines, opp_score, opp_lines, duration_s;
};

bool parse_summary(const uint8_t* p, size_t n, Summary& out) {
    if (n != 21) return false;
    out.won        = p[0];
    out.my_score   = net::le_read_u32(p + 1);
    out.my_lines   = net::le_read_u32(p + 5);
    out.opp_score  = net::le_read_u32(p + 9);
    out.opp_lines  = net::le_read_u32(p + 13);
    out.duration_s = net::le_read_u32(p + 17);
    return true;
}

std::vector<uint8_t> build_match_result(int32_t before, int32_t after, int32_t delta) {
    std::vector<uint8_t> pl;
    pl.reserve(12);
    net::le_write_u32(pl, (uint32_t)before);
    net::le_write_u32(pl, (uint32_t)after);
    net::le_write_u32(pl, (uint32_t)delta);
    return net::build_frame(net::MsgType::MATCH_RESULT, pl);
}

std::string extract_token(const std::vector<uint8_t>& pl, size_t off) {
    if (pl.size() < off + 1) return {};
    const uint8_t n = pl[off];
    if (n == 0 || pl.size() < off + 1u + n) return {};
    return std::string(pl.begin() + off + 1, pl.begin() + off + 1 + n);
}

// ── 연결 상태 ────────────────────────────────────────────────────────────────
struct Channel;

enum class Stage { FirstFrame, Auth, Queued, Lobby, Forward, Dead };

struct Conn {
    net::TcpSocket sock;
    int      fd = -1;
    uint32_t id = 0;
    Stage    stage = Stage::FirstFrame;

    std::vector<uint8_t> rx;   // 수신 누적(프레임 경계 파싱 전)
    std::vector<uint8_t> tx;   // 보류 송신(쓰기 준비성 대기)
    bool     want_write = false;
    bool     read_paused = false;   // 상대의 tx 가 차서 읽기를 멈춘 상태

    std::string admission_key;

    // 인증 결과
    int64_t     player_id = 0;
    int         elo = 0;
    std::string username, token, icon{"default"};
    std::shared_ptr<PlayerSessionLease> lease;

    // 매치
    Channel* ch = nullptr;
    bool     is_a = false;
    bool     ready = false;

    // 전달 한도
    TimePoint last_activity{};
    TimePoint byte_window_start{};
    size_t    byte_window = 0;
};

struct Channel {
    uint32_t    match_id = 0;
    std::string match_uuid;
    uint64_t    seed = 0;
    bool        ranked = false;

    Conn* a = nullptr;   // HOST — 죽으면 nullptr
    Conn* b = nullptr;   // GUEST

    // 소켓 복사본: Conn 이 사라진 뒤에도 MATCH_RESULT 를 보낼 수 있게 채널이
    // 참조 카운트를 하나 붙들고 있는다(TcpSocket 은 shared_ptr<int> 소유 핸들).
    net::TcpSocket sockA, sockB;

    int64_t a_id = 0, b_id = 0;
    int     a_elo = 0, b_elo = 0;
    std::shared_ptr<PlayerSessionLease> a_lease, b_lease;

    std::optional<Summary> sumA, sumB;
    bool summary_handled = false;
    bool finalize_inflight = false;
    int  disconnect_side = 0;  // 1=A, 2=B, 0=미상 — 승패가 아니라 통지 대상 선정용
};

// ── 루프 ─────────────────────────────────────────────────────────────────────
class RelayLoop {
public:
    RelayLoop(meta::client::MetaClient* meta, std::string meta_note)
        : meta_(meta), meta_note_(std::move(meta_note)) {}

    bool init(uint16_t port) {
        reactor_ = net::Reactor::create();
        if (!reactor_) {
            std::cerr << "[relay] reactor 생성 실패\n";
            return false;
        }
        offload_ = std::make_unique<Offload>(4, [this] { reactor_->wake(); });

        listen_ = net::tcp_listen(port, 64);
        if (!listen_.valid()) {
            std::cerr << "[relay] port " << port << " listen 실패\n";
            return false;
        }
        net::tcp_set_nonblocking(listen_);
        if (!reactor_->add(listen_.fd(), net::kRead, &listen_token_)) {
            std::cerr << "[relay] listen fd 등록 실패\n";
            return false;
        }
        std::cout << "[relay] reactor listening on 0.0.0.0:" << port << "\n";
        std::cout << "[relay] " << meta_note_ << "\n";
        return true;
    }

    void run() {
        std::vector<net::Event>   events;
        std::vector<void*>        expired;
        std::vector<Offload::Cont> conts;

        while (g_running.load()) {
            const TimePoint now = Clock::now();
            int timeout = timers_.timeout_ms(now);
            if (timeout < 0 || timeout > 500) timeout = 500;  // 종료 플래그 확인 주기

            const int n = reactor_->poll(events, timeout);
            if (n < 0) {
                std::cerr << "[relay] poll 오류 — 종료\n";
                break;
            }

            // 1) 오프로드 완료분을 루프 스레드에서 실행 (소켓 I/O 단일 스레드 유지)
            conts.clear();
            offload_->drain(conts);
            for (auto& c : conts) c();

            // 2) I/O 이벤트
            for (const auto& ev : events) {
                if (ev.token == &listen_token_) { on_accept(); continue; }
                Conn* c = static_cast<Conn*>(ev.token);
                if (!alive(c)) continue;          // 이번 배치에서 이미 죽은 연결
                if (ev.writable) on_writable(c);
                if (!alive(c)) continue;
                if (ev.readable || ev.error) on_readable(c);
            }

            // 3) 만기
            expired.clear();
            timers_.expired(Clock::now(), expired);
            for (void* t : expired) {
                Conn* c = static_cast<Conn*>(t);
                if (!alive(c)) continue;
                on_timeout(c);
            }

            sweep();
        }

        shutdown();
    }

private:
    // ── 수명 관리 ────────────────────────────────────────────────────────────
    bool alive(Conn* c) const {
        auto it = conns_.find(c);
        return it != conns_.end() && it->second->stage != Stage::Dead;
    }

    // 죽은 연결은 즉시 해제하지 않는다 — 같은 배치의 뒤쪽 이벤트가 이 포인터를
    // 들고 있을 수 있다. 표시만 하고 배치 끝(sweep)에서 해제한다.
    void close_conn(Conn* c, const char* why) {
        if (!c || c->stage == Stage::Dead) return;
        std::cerr << "[conn " << c->id << "] close: " << why << "\n";
        c->stage = Stage::Dead;
        reactor_->remove(c->fd);
        timers_.cancel(c);
        net::tcp_close(c->sock);
        release_admission(c->admission_key);
        c->lease.reset();

        if (c->ch) {
            Channel* ch = c->ch;
            (c->is_a ? ch->a : ch->b) = nullptr;
            if (ch->disconnect_side == 0) ch->disconnect_side = c->is_a ? 1 : 2;
            on_channel_peer_lost(ch);
        }
        // 큐 대기 중이었다면 큐에서도 뺀다.
        for (auto it = queue_.begin(); it != queue_.end(); ++it) {
            if (*it == c) { queue_.erase(it); break; }
        }
        dying_.push_back(c);
    }

    void sweep() {
        for (Conn* c : dying_) conns_.erase(c);
        dying_.clear();
        // 양쪽이 사라지고 finalize 도 끝난 채널을 정리한다.
        for (auto it = channels_.begin(); it != channels_.end();) {
            Channel* ch = it->second.get();
            if (!ch->a && !ch->b && !ch->finalize_inflight) it = channels_.erase(it);
            else ++it;
        }
    }

    void release_admission(const std::string& key) {
        if (key.empty()) return;
        auto it = admission_.find(key);
        if (it == admission_.end()) return;
        if (--it->second <= 0) admission_.erase(it);
    }

    // ── accept ───────────────────────────────────────────────────────────────
    void on_accept() {
        // 준비된 연결을 다 비운다 — 레벨 트리거라도 한 번에 처리하는 편이 낫다.
        for (;;) {
            net::TcpSocket s = net::tcp_accept(listen_);
            if (!s.valid()) return;

            if (conns_.size() >= kMaxConns) {
                std::cerr << "[relay] 연결 상한 도달 — 거절\n";
                net::tcp_close(s);
                continue;
            }
            std::string key = net::tcp_peer_ip(s);
            if (key.empty()) key = "fd:" + std::to_string(s.fd());  // 공멸 방지
            if (admission_[key] >= kMaxHandshakesPerIp) {
                std::cerr << "[relay] per-IP handshake 상한 (" << key << ") — 거절\n";
                admission_.erase(key);  // 방금 만든 0 항목이면 지운다
                net::tcp_close(s);
                continue;
            }
            ++admission_[key];

            auto c = std::make_unique<Conn>();
            c->sock = std::move(s);
            c->fd   = c->sock.fd();
            c->id   = next_conn_id_++;
            c->admission_key = key;
            c->last_activity = Clock::now();
            Conn* raw = c.get();
            if (!reactor_->add(raw->fd, net::kRead, raw)) {
                std::cerr << "[relay] fd 등록 실패 — 거절\n";
                release_admission(key);
                net::tcp_close(raw->sock);
                continue;
            }
            timers_.arm(raw, Clock::now() + kFirstFrameTimeout);
            conns_[raw] = std::move(c);
            std::cerr << "[conn " << raw->id << "] accepted from " << key << "\n";
        }
    }

    // ── 송신(backpressure) ───────────────────────────────────────────────────
    // 논블로킹으로 최대한 보내고 남는 것은 tx 에 쌓는다. 상대가 안 읽어 tx 가
    // 한계를 넘으면 그 소켓으로 흘려보내는 쪽의 읽기를 멈춘다.
    bool queue_send(Conn* dst, const uint8_t* data, size_t len) {
        if (!dst || dst->stage == Stage::Dead) return false;
        size_t sent = 0;
        if (dst->tx.empty()) {
            if (!net::tcp_send_some(dst->sock, data, len, sent)) return false;
        }
        if (sent < len) {
            dst->tx.insert(dst->tx.end(), data + sent, data + len);
            arm_write(dst, true);
            if (dst->tx.size() > kSendHighWater) pause_peer_read(dst, true);
        }
        return true;
    }

    void arm_write(Conn* c, bool want) {
        if (c->want_write == want) return;
        c->want_write = want;
        unsigned interest = (c->read_paused ? 0u : net::kRead) |
                            (want ? net::kWrite : 0u);
        reactor_->modify(c->fd, interest, c);
    }

    void pause_peer_read(Conn* dst, bool pause) {
        Channel* ch = dst->ch;
        if (!ch) return;
        Conn* src = (dst == ch->a) ? ch->b : ch->a;
        if (!src || src->stage == Stage::Dead) return;
        if (src->read_paused == pause) return;
        src->read_paused = pause;
        unsigned interest = (pause ? 0u : net::kRead) |
                            (src->want_write ? net::kWrite : 0u);
        reactor_->modify(src->fd, interest, src);
    }

    void on_writable(Conn* c) {
        if (c->tx.empty()) { arm_write(c, false); return; }
        size_t sent = 0;
        if (!net::tcp_send_some(c->sock, c->tx.data(), c->tx.size(), sent)) {
            close_conn(c, "send 실패");
            return;
        }
        if (sent) c->tx.erase(c->tx.begin(), c->tx.begin() + sent);
        if (c->tx.empty()) {
            arm_write(c, false);
            pause_peer_read(c, false);   // 밀림이 풀렸으니 상대 읽기 재개
        } else if (c->tx.size() <= kSendHighWater) {
            pause_peer_read(c, false);
        }
    }

    // ── 수신 ─────────────────────────────────────────────────────────────────
    void on_readable(Conn* c) {
        const size_t before = c->rx.size();
        if (!net::tcp_recv_some(c->sock, c->rx)) {
            close_conn(c, "peer 종료");
            return;
        }
        const size_t got = c->rx.size() - before;
        if (got == 0) return;   // 준비성만 왔고 실제 데이터는 없음

        const TimePoint now = Clock::now();
        c->last_activity = now;
        if (now - c->byte_window_start >= std::chrono::seconds(1)) {
            c->byte_window_start = now;
            c->byte_window = 0;
        }
        c->byte_window += got;
        if (c->stage == Stage::Forward && c->byte_window > kMaxBytesPerSecond) {
            close_conn(c, "byte rate 초과");
            return;
        }

        switch (c->stage) {
            case Stage::FirstFrame: on_first_frame(c); break;
            case Stage::Lobby:      on_lobby(c);       break;
            case Stage::Forward:    on_forward(c);     break;
            case Stage::Queued:     on_queued(c);      break;
            case Stage::Auth:       break;  // 인증 중 — rx 에 쌓아 두고 나중에 처리
            case Stage::Dead:       break;
        }
    }

    // 첫 프레임: QUEUE_JOIN 만 이관됐다. 인증은 오프로드한다.
    void on_first_frame(Conn* c) {
        std::vector<net::Frame> frames;
        net::parse_frames(c->rx, frames);
        for (size_t i = 0; i < frames.size(); ++i) {
            const net::Frame& f = frames[i];
            if (f.type == net::MsgType::QUEUE_JOIN) {
                std::string tok = extract_token(f.payload, 0);
                // 첫 명령 뒤에 이미 도착한 프레임/부분 바이트를 보존한다.
                std::vector<uint8_t> residual;
                for (size_t j = i + 1; j < frames.size(); ++j) {
                    auto bytes = net::build_frame(frames[j].type, frames[j].payload);
                    residual.insert(residual.end(), bytes.begin(), bytes.end());
                }
                residual.insert(residual.end(), c->rx.begin(), c->rx.end());
                c->rx = std::move(residual);
                begin_auth(c, std::move(tok));
                return;
            }
            if (f.type == net::MsgType::QUEUE_CANCEL) {
                close_conn(c, "큐 진입 전 QUEUE_CANCEL");
                return;
            }
            if (f.type == net::MsgType::ROOM_CREATE || f.type == net::MsgType::ROOM_JOIN) {
                // 룸 경로는 아직 이 바이너리에 이관되지 않았다.
                close_conn(c, "room 경로 미지원(reactor 빌드)");
                return;
            }
            // HELLO 등 낯선 프레임은 무시하고 계속 기다린다.
        }
    }

    // 인증은 meta HTTP 왕복이라 루프에서 부르면 그동안 전원이 멈춘다 — 워커로 뺀다.
    void begin_auth(Conn* c, std::string token) {
        c->stage = Stage::Auth;
        timers_.cancel(c);

        if (!meta_) {                    // unranked: 검증 없이 통과
            c->token = std::move(token);
            enter_queue(c);
            return;
        }
        if (token.empty()) {
            close_conn(c, "토큰 없음 -> 거절");
            return;
        }

        // continuation 은 Conn* 이 아니라 conn id 를 포착한다 — 인증 왕복 사이에
        // 그 연결이 끊겨 객체가 사라졌을 수 있기 때문이다. 재개 시점에 id 로 다시
        // 찾고, 없으면 조용히 버린다.
        const uint32_t cid = c->id;
        meta::client::MetaClient* meta = meta_;
        const bool queued = offload_->submit(
            [this, meta, token, cid]() -> Offload::Cont {
                meta::client::MetaClient::VerifyOutcome outcome{};
                auto auth = meta->verify_token(token, 3, &outcome);
                return [this, cid, auth, token]() { resume_auth(cid, auth, token); };
            });
        if (!queued) close_conn(c, "종료 중 — 인증 불가");
    }

    void resume_auth(uint32_t conn_id,
                     std::optional<meta::client::AuthInfo> auth,
                     const std::string& token) {
        pending_auth_.erase(conn_id);
        Conn* c = find_by_id(conn_id);
        if (!c) return;   // 인증 도중 끊겼다
        if (!auth) {
            close_conn(c, "meta verify 실패 -> 거절");
            return;
        }
        c->player_id = auth->player_id;
        c->elo       = auth->elo;
        c->username  = auth->username;
        c->token     = token;
        c->icon      = auth->selected_icon_id.empty() ? "default" : auth->selected_icon_id;
        c->lease     = PlayerSessionLease::acquire(c->player_id);
        if (!c->lease) {
            close_conn(c, "동일 player_id 중복 세션 -> 거절");
            return;
        }
        std::cerr << "[conn " << c->id << "] authed player_id=" << c->player_id
                  << " elo=" << c->elo << "\n";
        enter_queue(c);
    }

    Conn* find_by_id(uint32_t id) {
        for (auto& [ptr, up] : conns_) {
            if (up->id == id && up->stage != Stage::Dead) return ptr;
        }
        return nullptr;
    }

    // ── 큐 ───────────────────────────────────────────────────────────────────
    void enter_queue(Conn* c) {
        c->stage = Stage::Queued;
        queue_.push_back(c);
        std::cerr << "[conn " << c->id << "] queued (" << queue_.size() << " 대기)\n";
        try_pair();
        // 큐에 남았다면 이미 도착한 QUEUE_CANCEL 을 지금 처리한다.
        if (c->stage == Stage::Queued && !c->rx.empty()) on_queued(c);
    }

    // 큐 대기 중에는 QUEUE_CANCEL 만 본다. 그 외 바이트는 쌓아 두고 매치 성립 시
    // 로비 버퍼로 넘어간다(프레임이 세그먼트 경계에 걸쳐도 유실되지 않게).
    void on_queued(Conn* c) {
        std::vector<uint8_t> copy = c->rx;
        std::vector<net::Frame> frames;
        net::parse_frames(copy, frames);
        for (const auto& f : frames) {
            if (f.type == net::MsgType::QUEUE_CANCEL) {
                close_conn(c, "QUEUE_CANCEL");
                return;
            }
        }
    }

    void try_pair() {
        while (queue_.size() >= 2) {
            Conn* a = queue_.front(); queue_.pop_front();
            Conn* b = queue_.front(); queue_.pop_front();
            if (!alive(a)) { if (alive(b)) queue_.push_front(b); continue; }
            if (!alive(b)) { queue_.push_front(a); continue; }
            start_match(a, b);
        }
    }

    uint64_t next_seed() {
        // xorshift64 — 서버 내부 RNG (스레드 모델과 같은 방식)
        if (seed_state_ == 0) seed_state_ = 0x9E3779B97F4A7C15ULL ^ (uint64_t)Clock::now().time_since_epoch().count();
        seed_state_ ^= seed_state_ << 13;
        seed_state_ ^= seed_state_ >> 7;
        seed_state_ ^= seed_state_ << 17;
        return seed_state_;
    }

    void start_match(Conn* a, Conn* b) {
        auto up = std::make_unique<Channel>();
        Channel* ch = up.get();
        ch->match_id   = next_match_id_++;
        ch->match_uuid = new_match_uuid();
        ch->seed       = next_seed();
        ch->a = a; ch->b = b;
        ch->sockA = a->sock; ch->sockB = b->sock;
        ch->a_id = a->player_id; ch->b_id = b->player_id;
        ch->a_elo = a->elo;      ch->b_elo = b->elo;
        ch->a_lease = a->lease;  ch->b_lease = b->lease;
        ch->ranked = (meta_ != nullptr) && a->player_id != 0 && b->player_id != 0;
        channels_[ch->match_id] = std::move(up);

        a->ch = ch; a->is_a = true;  a->stage = Stage::Lobby;
        b->ch = ch; b->is_a = false; b->stage = Stage::Lobby;

        std::cerr << "[relay] match=" << ch->match_id << " paired conn "
                  << a->id << " x " << b->id
                  << (ch->ranked ? " (ranked)" : " (unranked)") << "\n";

        if (!send_match_found(a, 1, ch->seed, a->icon, b->icon, ch->match_uuid) ||
            !send_match_found(b, 2, ch->seed, b->icon, a->icon, ch->match_uuid)) {
            close_conn(a, "MATCH_FOUND 송신 실패");
            close_conn(b, "MATCH_FOUND 송신 실패");
            return;
        }
        const TimePoint deadline = Clock::now() + kLobbyTimeout;
        timers_.arm(a, deadline);
        timers_.arm(b, deadline);

        // 매칭 직전에 이미 도착해 있던 바이트를 로비 규칙으로 처리한다.
        if (!a->rx.empty()) on_lobby(a);
        if (alive(b) && !b->rx.empty()) on_lobby(b);
    }

    bool send_match_found(Conn* c, uint8_t role, uint64_t seed,
                          const std::string& my_icon, const std::string& peer_icon,
                          const std::string& uuid) {
        const std::string my   = my_icon.empty()   ? "default" : my_icon;
        const std::string peer = peer_icon.empty() ? "default" : peer_icon;
        std::vector<uint8_t> pl;
        pl.push_back(role);
        net::le_write_u64(pl, seed);
        pl.push_back((uint8_t)std::min<size_t>(my.size(), 255));
        pl.insert(pl.end(), my.begin(), my.begin() + std::min<size_t>(my.size(), 255));
        pl.push_back((uint8_t)std::min<size_t>(peer.size(), 255));
        pl.insert(pl.end(), peer.begin(), peer.begin() + std::min<size_t>(peer.size(), 255));
        pl.push_back((uint8_t)std::min<size_t>(uuid.size(), 255));
        pl.insert(pl.end(), uuid.begin(), uuid.begin() + std::min<size_t>(uuid.size(), 255));
        auto fr = net::build_frame(net::MsgType::MATCH_FOUND, pl);
        return queue_send(c, fr.data(), fr.size());
    }

    // ── 로비(READY 핸드셰이크) ───────────────────────────────────────────────
    void on_lobby(Conn* c) {
        Channel* ch = c->ch;
        if (!ch) return;
        Conn* peer = c->is_a ? ch->b : ch->a;

        if (c->rx.size() > kMaxLobbyBufBytes) {
            close_conn(c, "로비 버퍼 초과");
            return;
        }

        // READY/QUEUE_CANCEL 만 소비한다. 게임 프레임을 만나면 멈추고 rx 에 남겨
        // 포워딩 단계가 그대로 이어받는다.
        while (c->rx.size() >= 6 && !c->ready) {
            const uint16_t len = (uint16_t)c->rx[0] | ((uint16_t)c->rx[1] << 8);
            if ((size_t)len > net::kMaxPayloadBytes + 1u) {
                close_conn(c, "로비 프레임 길이 초과");
                return;
            }
            const size_t total = 2u + (size_t)len + 4u;
            if (c->rx.size() < total) break;      // 미완성 — 더 기다린다
            if (len < 1) { c->rx.erase(c->rx.begin(), c->rx.begin() + total); continue; }

            const uint8_t type = c->rx[2];
            if (type != (uint8_t)net::MsgType::READY &&
                type != (uint8_t)net::MsgType::QUEUE_CANCEL) {
                break;   // 게임 프레임 — 포워딩으로 넘긴다
            }
            const size_t payload_len = (size_t)len - 1u;
            const uint32_t chk  = net::le_read_u32(c->rx.data() + 2u + len);
            const uint32_t calc = payload_len == 0 ? 0u
                                : net::fnv1a32(c->rx.data() + 3, payload_len);
            if (chk != calc) { c->rx.erase(c->rx.begin(), c->rx.begin() + total); continue; }

            const bool is_ready = (type == (uint8_t)net::MsgType::READY);
            const uint8_t v = (is_ready && payload_len >= 1) ? c->rx[3] : 0;
            c->rx.erase(c->rx.begin(), c->rx.begin() + total);

            if (!is_ready || v == 0) {           // 취소/거절 — 상대에게 알리고 종료
                if (peer && peer->stage != Stage::Dead) {
                    std::vector<uint8_t> pl{0};
                    auto fr = net::build_frame(net::MsgType::READY, pl);
                    queue_send(peer, fr.data(), fr.size());
                }
                close_conn(c, "로비에서 취소/거절");
                return;
            }
            // READY(1) — 상대에게 전달
            c->ready = true;
            if (peer && peer->stage != Stage::Dead) {
                std::vector<uint8_t> pl{1};
                auto fr = net::build_frame(net::MsgType::READY, pl);
                if (!queue_send(peer, fr.data(), fr.size())) {
                    close_conn(peer, "READY 전달 실패");
                    return;
                }
            }
        }

        if (ch->a && ch->b && ch->a->ready && ch->b->ready) begin_forwarding(ch);
    }

    void begin_forwarding(Channel* ch) {
        Conn* a = ch->a; Conn* b = ch->b;
        if (!a || !b) return;
        const TimePoint now = Clock::now();
        for (Conn* c : {a, b}) {
            c->stage = Stage::Forward;
            c->last_activity = now;
            c->byte_window_start = now;
            c->byte_window = 0;
            timers_.arm(c, now + kIdleTimeout);
        }
        std::cerr << "[relay] match=" << ch->match_id << " forwarding 시작\n";
        // 로비에서 남은 바이트(READY 이후 도착한 게임 프레임)를 지금 흘려보낸다.
        if (!a->rx.empty()) on_forward(a);
        if (alive(b) && !b->rx.empty()) on_forward(b);
    }

    // ── 포워딩 ───────────────────────────────────────────────────────────────
    void on_forward(Conn* c) {
        Channel* ch = c->ch;
        if (!ch) return;
        Conn* peer = c->is_a ? ch->b : ch->a;
        timers_.arm(c, Clock::now() + kIdleTimeout);

        if (!ch->ranked) {
            // unranked: 프레임 경계도 만들지 않고 받은 바이트를 그대로 전달한다.
            if (!c->rx.empty()) {
                if (!queue_send(peer, c->rx.data(), c->rx.size())) {
                    close_conn(peer ? peer : c, "전달 실패");
                    return;
                }
                c->rx.clear();
            }
            return;
        }

        // ranked: MATCH_SUMMARY 만 가로채고 나머지는 원본 바이트 그대로 전달한다.
        size_t consumed = 0;
        while (c->rx.size() - consumed >= 2) {
            const uint8_t* p = c->rx.data() + consumed;
            const size_t avail = c->rx.size() - consumed;
            const uint16_t len = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
            if ((size_t)len > net::kMaxPayloadBytes + 1u) {
                std::cerr << "[relay] match=" << ch->match_id
                          << " 과대 프레임 — 스트림 폐기\n";
                consumed = c->rx.size();
                break;
            }
            const size_t total = 2u + (size_t)len + 4u;
            if (avail < total) break;
            if (len < 1) { consumed += total; continue; }

            if (p[2] == (uint8_t)net::MsgType::MATCH_SUMMARY) {
                const size_t payload_len = (size_t)len - 1u;
                const uint32_t chk  = net::le_read_u32(p + 2u + len);
                const uint32_t calc = payload_len == 0 ? 0u : net::fnv1a32(p + 3, payload_len);
                Summary s{};
                if (chk == calc && parse_summary(p + 3, payload_len, s)) {
                    auto& slot = c->is_a ? ch->sumA : ch->sumB;
                    if (!slot) slot = s;
                    std::cerr << "[relay] match=" << ch->match_id << " MATCH_SUMMARY from "
                              << (c->is_a ? "A" : "B") << " won=" << (int)s.won << "\n";
                }
                consumed += total;   // 가로챔 — 상대에게 보내지 않는다
                continue;
            }
            if (!queue_send(peer, p, total)) {
                close_conn(peer ? peer : c, "전달 실패");
                return;
            }
            consumed += total;
        }
        if (consumed) c->rx.erase(c->rx.begin(), c->rx.begin() + consumed);

        if (ch->sumA && ch->sumB && !ch->summary_handled) finalize_ranked(ch);
    }

    // ── 만기 ─────────────────────────────────────────────────────────────────
    void on_timeout(Conn* c) {
        switch (c->stage) {
            case Stage::FirstFrame: close_conn(c, "첫 프레임 타임아웃"); break;
            case Stage::Lobby:      close_conn(c, "로비 타임아웃");      break;
            case Stage::Forward:    close_conn(c, "idle 타임아웃");      break;
            default: break;
        }
    }

    // ── finalize ─────────────────────────────────────────────────────────────
    // 상대가 사라졌다. 요약 수집 상태에 따라 세 갈래 — 스레드 모델과 같은 정책이다.
    void on_channel_peer_lost(Channel* ch) {
        if (!ch->ranked || ch->summary_handled || ch->finalize_inflight) return;
        if (ch->sumA && ch->sumB) { finalize_ranked(ch); return; }
        if (!ch->sumA && !ch->sumB) {
            // 무경기 — meta 에 보내지 않는다(담합 RP 파밍·동시 단절 오염 차단).
            ch->summary_handled = true;
            send_result_frames(ch, ch->a_elo, ch->a_elo, 0, ch->b_elo, ch->b_elo, 0);
            std::cerr << "[relay] match=" << ch->match_id
                      << " 요약 없음 -> meta 미전송 (delta=0)\n";
            return;
        }
        finalize_forfeit(ch);
    }

    void finalize_ranked(Channel* ch) {
        if (ch->summary_handled || ch->finalize_inflight) return;
        ch->summary_handled = true;
        const Summary a = *ch->sumA, b = *ch->sumB;
        const bool exclusive = (a.won ^ b.won) != 0;
        const bool scores_ok = (a.my_score == b.opp_score) && (b.my_score == a.opp_score);
        const bool lines_ok  = (a.my_lines == b.opp_lines) && (b.my_lines == a.opp_lines);
        std::optional<int64_t> winner;
        if (exclusive && scores_ok && lines_ok) {
            winner = (a.won == 1) ? ch->a_id : ch->b_id;
        } else {
            std::cerr << "[relay] match=" << ch->match_id
                      << " 교차검증 실패 -> winner=null\n";
        }
        post_result(ch, winner, (int)a.my_score, (int)b.my_score,
                    (int)a.my_lines, (int)b.my_lines,
                    (int)std::max(a.duration_s, b.duration_s));
    }

    void finalize_forfeit(Channel* ch) {
        ch->summary_handled = true;
        const bool haveA = ch->sumA.has_value();
        const Summary s = haveA ? *ch->sumA : *ch->sumB;
        // 한쪽 요약만 있으면 그 요약의 won 을 존중한다 — 끊긴 순서로 승자를 정하면
        // 승리 요약을 낸 직후 회선이 끊긴 쪽이 패자로 뒤집힌다.
        const int64_t winner = haveA ? (s.won ? ch->a_id : ch->b_id)
                                     : (s.won ? ch->b_id : ch->a_id);
        const int scoreA = (int)(haveA ? s.my_score : s.opp_score);
        const int scoreB = (int)(haveA ? s.opp_score : s.my_score);
        const int linesA = (int)(haveA ? s.my_lines : s.opp_lines);
        const int linesB = (int)(haveA ? s.opp_lines : s.my_lines);
        post_result(ch, winner, scoreA, scoreB, linesA, linesB, (int)s.duration_s);
    }

    // meta POST 는 블로킹이라 워커로 뺀다. 결과 프레임 송신은 루프가 한다.
    void post_result(Channel* ch, std::optional<int64_t> winner,
                     int sa, int sb, int la, int lb, int dur) {
        if (!meta_) return;
        ch->finalize_inflight = true;
        const uint32_t mid = ch->match_id;
        meta::client::MetaClient* meta = meta_;
        const std::string uuid = ch->match_uuid;
        const int64_t aid = ch->a_id, bid = ch->b_id;

        const bool queued = offload_->submit(
            [this, meta, uuid, aid, bid, winner, sa, sb, la, lb, dur, mid]() -> Offload::Cont {
                auto res = meta->post_match(uuid, aid, bid, winner, sa, sb, la, lb, dur);
                return [this, mid, res]() { on_result_saved(mid, res); };
            });
        if (!queued) {
            // 종료 중이라 저장할 수 없다 — 결과를 삼키지 말고 남긴다.
            std::cerr << "[relay] match=" << mid
                      << " 종료 중 — meta 저장 생략\n";
            ch->finalize_inflight = false;
        }
    }

    void on_result_saved(uint32_t match_id,
                         std::optional<meta::client::MatchResult> res) {
        auto it = channels_.find(match_id);
        if (it == channels_.end()) return;
        Channel* ch = it->second.get();
        ch->finalize_inflight = false;
        if (res) {
            send_result_frames(ch, res->a.elo_before, res->a.elo_after, res->a.delta,
                                   res->b.elo_before, res->b.elo_after, res->b.delta);
            std::cerr << "[relay] match=" << match_id << " meta 저장 완료\n";
        } else {
            send_result_frames(ch, ch->a_elo, ch->a_elo, 0, ch->b_elo, ch->b_elo, 0);
            std::cerr << "[relay] match=" << match_id << " meta POST 실패 — delta=0\n";
        }
    }

    // 채널이 붙들고 있는 소켓 복사본으로 보낸다 — Conn 이 이미 사라졌어도 된다.
    void send_result_frames(Channel* ch, int ab, int aa, int ad, int bb, int ba, int bd) {
        auto frA = build_match_result(ab, aa, ad);
        auto frB = build_match_result(bb, ba, bd);
        size_t sent = 0;
        if (ch->disconnect_side != 1 && ch->sockA.valid())
            net::tcp_send_some(ch->sockA, frA.data(), frA.size(), sent);
        if (ch->disconnect_side != 2 && ch->sockB.valid())
            net::tcp_send_some(ch->sockB, frB.data(), frB.size(), sent);
    }

    // ── 종료 ─────────────────────────────────────────────────────────────────
    void shutdown() {
        std::cout << "[relay] shutting down...\n";
        // 새 job 을 막고 이미 큐에 있는 것(진짜 끝난 경기의 결과 저장)은 마친다.
        // 큐 깊이는 그 순간 종료된 매치 수로 한정되고, 새 연결을 받지 않으므로
        // 드레인 중에 자라지 않는다.
        offload_->shutdown();
        std::vector<Offload::Cont> conts;
        offload_->drain(conts);
        for (auto& c : conts) c();

        for (auto& [ptr, up] : conns_) {
            if (up->stage != Stage::Dead) net::tcp_close(up->sock);
        }
        conns_.clear();
        channels_.clear();
        net::tcp_close(listen_);
        std::cout << "[relay] done\n";
    }

    meta::client::MetaClient* meta_ = nullptr;
    std::string meta_note_;

    std::unique_ptr<net::Reactor> reactor_;
    std::unique_ptr<Offload>      offload_;
    TimerQueue                    timers_;

    net::TcpSocket listen_;
    char           listen_token_ = 0;

    std::unordered_map<Conn*, std::unique_ptr<Conn>>     conns_;
    std::unordered_map<uint32_t, std::unique_ptr<Channel>> channels_;
    std::deque<Conn*>          queue_;
    std::vector<Conn*>         dying_;
    std::unordered_map<std::string, int> admission_;
    std::unordered_set<uint32_t> pending_auth_;

    uint32_t next_conn_id_  = 1;
    uint32_t next_match_id_ = 1;
    uint64_t seed_state_    = 0;
};

} // namespace
} // namespace relay

int main(int argc, char** argv) {
    uint16_t port = 7777;
    std::string meta_url, meta_secret;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "[relay] " << what << " 인자 누락\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port")             port = (uint16_t)std::stoi(next("--port"));
        else if (a == "--meta")        meta_url = next("--meta");
        else if (a == "--meta-secret") meta_secret = next("--meta-secret");
        else if (a == "-h" || a == "--help") {
            std::cout << "Usage: tetris_relay_reactor [--port N] [--meta URL]"
                         " [--meta-secret S]\n"
                         "  단일 이벤트 루프(epoll/IOCP) 릴레이. 큐 경로만 지원.\n";
            return 0;
        }
    }
    if (meta_secret.empty()) {
        if (const char* env = std::getenv("TETRIS_RELAY_SECRET")) meta_secret = env;
    }

    std::signal(SIGINT,  relay::on_signal);
    std::signal(SIGTERM, relay::on_signal);
#if defined(_WIN32)
    std::signal(SIGBREAK, relay::on_signal);
#endif

    if (!net::net_init()) {
        std::cerr << "[relay] net_init 실패\n";
        return 1;
    }

    std::unique_ptr<meta::client::MetaClient> meta;
    std::string note = "meta=none (unranked mode)";
    if (!meta_url.empty()) {
        // secret 없이 ranked 로 뜨면 meta 가 POST /v1/matches 를 거절하므로 경기
        // 결과를 하나도 저장하지 못한다. 그 상태로 조용히 도는 대신 기동을 거부한다.
        if (meta_secret.empty()) {
            std::cerr << "[relay] refusing to start: --meta set but no relay secret. "
                      << "Set --meta-secret or TETRIS_RELAY_SECRET (meta rejects "
                      << "POST /v1/matches without it).\n";
            net::net_shutdown();
            return 2;
        }
        meta = std::make_unique<meta::client::MetaClient>(meta_url, meta_secret);
        if (!meta->valid()) {
            std::cerr << "[relay] invalid --meta URL: " << meta_url << "\n";
            net::net_shutdown();
            return 2;
        }
        note = "meta=" + meta_url;
    }

    relay::RelayLoop loop(meta.get(), note);
    int rc = 0;
    if (!loop.init(port)) rc = 1;
    else loop.run();

    net::net_shutdown();
    return rc;
}
