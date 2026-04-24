#include "relay.h"

#include "../net/framing.h"
#include "../net/socket.h"
#include "../meta/http_client.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

namespace {

// MATCH_SUMMARY 페이로드 구조 (net/framing.h 에 명시된 정확히 21 바이트):
//   [won:1][my_score:4 LE][my_lines:4 LE]
//   [opp_score_observed:4 LE][opp_lines_observed:4 LE]
//   [duration_s:4 LE]
struct Summary {
    uint8_t  won;
    uint32_t my_score;
    uint32_t my_lines;
    uint32_t opp_score;
    uint32_t opp_lines;
    uint32_t duration_s;
};

bool parse_summary(const std::vector<uint8_t>& p, Summary& out)
{
    if (p.size() != 21) return false;
    out.won        = p[0];
    out.my_score   = net::le_read_u32(&p[1]);
    out.my_lines   = net::le_read_u32(&p[5]);
    out.opp_score  = net::le_read_u32(&p[9]);
    out.opp_lines  = net::le_read_u32(&p[13]);
    out.duration_s = net::le_read_u32(&p[17]);
    return true;
}

std::vector<uint8_t> build_match_result(int32_t elo_before, int32_t elo_after, int32_t delta)
{
    std::vector<uint8_t> pl;
    pl.reserve(12);
    net::le_write_u32(pl, static_cast<uint32_t>(elo_before));
    net::le_write_u32(pl, static_cast<uint32_t>(elo_after));
    net::le_write_u32(pl, static_cast<uint32_t>(delta));
    return net::build_frame(net::MsgType::MATCH_RESULT, pl);
}

// 양 방향 스레드가 공유하는 채널 상태.
// · forwarder_count 가 0 이 되는 순간 양 소켓 close.
// · summaryA/B 는 forwarderLoop 가 MATCH_SUMMARY 프레임을 가로챌 때 채워짐.
struct Channel {
    net::TcpSocket   A;            // HOST 소켓
    net::TcpSocket   B;            // GUEST 소켓
    uint32_t         match_id{0};

    int64_t          playerA_id{0};
    int64_t          playerB_id{0};
    int              playerA_elo{1200};
    int              playerB_elo{1200};

    std::atomic<bool> closed{false};
    std::atomic<int>  forwarder_count{2};

    // MATCH_SUMMARY 수집
    std::mutex              sumMu;
    std::optional<Summary>  summaryA;
    std::optional<Summary>  summaryB;
    bool                    summaryHandled{false};   // 한 번만 처리

    // Lobby 단계에서 recv 됐지만 아직 포워딩되지 못한 raw 바이트.
    //   READY 교환 중 상대가 먼저 게임 프레임(PING 등)을 보내면 TCP 버퍼를 lobby
    //   스레드가 이미 kernel→userspace 로 끌어온 상태다. 그 바이트는 forwarder 가
    //   다시 recv 할 수 없으므로, 첫 iteration 에서 streamBuf / 상대 소켓으로 재주입.
    std::vector<uint8_t>   prefixFromA;
    std::vector<uint8_t>   prefixFromB;

    // 목적지 소켓별 send mutex — forwarderLoop 두 방향이 같은 목적지에 동시
    // tcp_send_all 을 호출하는 것을 직렬화.  배경:
    //   · A→B forwarder 는 B 에 tcp_send_all.
    //   · B→A forwarder 는 A 에 tcp_send_all.
    //   · finalizeRanked 는 A 와 B 양쪽에 MATCH_RESULT 를 직접 송신.
    // tcp_send_all 은 partial send 루프라 두 스레드가 같은 fd 에 interleaved 로
    // 진입하면 프레임 바이트가 섞일 위험이 있다 — 손상된 프레임 → 체크섬 실패 →
    // 그 프레임만 드롭되면 그나마 낫지만, MATCH_RESULT 같이 재전송이 없는 건
    // 유실된다. 목적지별 mutex 로 원자성 보장.
    std::mutex             sendMuA;
    std::mutex             sendMuB;

    // meta 호출 경로 (nullptr 이면 MATCH_SUMMARY 는 투명 포워딩)
    meta::client::MetaClient* meta{nullptr};
};

// A/B 소켓 각각에 대한 send — 대상별 mutex 로 직렬화.
bool sendToA(Channel& ch, const std::vector<uint8_t>& frame)
{
    std::lock_guard<std::mutex> lk(ch.sendMuA);
    return net::tcp_send_all(ch.A, frame.data(), frame.size());
}
bool sendToA(Channel& ch, const uint8_t* data, size_t len)
{
    std::lock_guard<std::mutex> lk(ch.sendMuA);
    return net::tcp_send_all(ch.A, data, len);
}
bool sendToB(Channel& ch, const std::vector<uint8_t>& frame)
{
    std::lock_guard<std::mutex> lk(ch.sendMuB);
    return net::tcp_send_all(ch.B, frame.data(), frame.size());
}
bool sendToB(Channel& ch, const uint8_t* data, size_t len)
{
    std::lock_guard<std::mutex> lk(ch.sendMuB);
    return net::tcp_send_all(ch.B, data, len);
}

// 두 MATCH_SUMMARY 가 모두 도착했을 때 교차검증 + meta POST + MATCH_RESULT 송신.
// 양방향 forwarderLoop 중 먼저 양쪽 수집 완료를 본 스레드 하나가 실행한다.
// 실패/meta-down 상황에서도
// 양 클라에 MATCH_RESULT(delta=0) 는 반드시 송신해 "ranking offline" 표시 가능.
void finalizeRanked(Channel& ch)
{
    // 선점 — 한 번만 실행.
    {
        std::lock_guard<std::mutex> lk(ch.sumMu);
        if (ch.summaryHandled) return;
        if (!ch.summaryA || !ch.summaryB) return;
        ch.summaryHandled = true;
    }
    const Summary a = *ch.summaryA;
    const Summary b = *ch.summaryB;

    // 교차검증 (계획문서 규칙):
    //   1) 한 명만 승리 주장해야 한다 (won_a XOR won_b).
    //   2) a.my_score == b.opp_score_observed 이고 반대도 성립.
    //   3) 라인수도 동일.
    const bool exclusive_win = (a.won ^ b.won) != 0;
    const bool scores_match  = (a.my_score == b.opp_score) && (b.my_score == a.opp_score);
    const bool lines_match   = (a.my_lines == b.opp_lines) && (b.my_lines == a.opp_lines);
    const bool cross_ok      = exclusive_win && scores_match && lines_match;

    std::optional<int64_t> winner;
    if (cross_ok) {
        winner = (a.won == 1) ? ch.playerA_id : ch.playerB_id;
    }
    if (!cross_ok) {
        std::cerr << "[relay] match=" << ch.match_id
                  << " cross-check FAIL (exclusive_win=" << exclusive_win
                  << " scores=" << scores_match
                  << " lines=" << lines_match
                  << ") -> winner=null\n";
    }

    // cross_ok=false 여도 감사 목적으로 자가보고 값을 그대로 기록한다.
    // winner=null 이므로 ELO 에는 영향 없음 — DB 에는 "누가 뭐라고 주장했나" 만 남는다.
    const int      duration_s = static_cast<int>(std::max(a.duration_s, b.duration_s));
    const int      score_a    = static_cast<int>(a.my_score);
    const int      score_b    = static_cast<int>(b.my_score);
    const int      lines_a    = static_cast<int>(a.my_lines);
    const int      lines_b    = static_cast<int>(b.my_lines);

    int deltaA = 0, deltaB = 0;
    int eloABefore = ch.playerA_elo, eloAAfter = ch.playerA_elo;
    int eloBBefore = ch.playerB_elo, eloBAfter = ch.playerB_elo;

    if (ch.meta) {
        auto res = ch.meta->post_match(ch.playerA_id, ch.playerB_id, winner,
                                       score_a, score_b, lines_a, lines_b,
                                       duration_s);
        if (res) {
            eloABefore = res->a.elo_before; eloAAfter = res->a.elo_after; deltaA = res->a.delta;
            eloBBefore = res->b.elo_before; eloBAfter = res->b.elo_after; deltaB = res->b.delta;
            std::cerr << "[relay] match=" << ch.match_id
                      << " saved meta match=" << res->match_id
                      << " a=" << (deltaA >= 0 ? "+" : "") << deltaA
                      << " b=" << (deltaB >= 0 ? "+" : "") << deltaB << "\n";
        } else {
            std::cerr << "[relay] match=" << ch.match_id
                      << " meta POST failed — MATCH_RESULT delta=0\n";
        }
    } else {
        std::cerr << "[relay] match=" << ch.match_id
                  << " no meta — MATCH_RESULT delta=0\n";
    }

    // MATCH_RESULT 송신 — 성공 실패 관계없이 양 클라에 한 번씩.
    // 반대 방향 forwarderLoop 가 동시에 같은 소켓에 쓰고 있을 수 있으므로 sendMuA/B
    // 로 직렬화.
    auto frA = build_match_result(eloABefore, eloAAfter, deltaA);
    auto frB = build_match_result(eloBBefore, eloBAfter, deltaB);
    sendToA(ch, frA);
    sendToB(ch, frB);
}

// 한 방향 포워딩 루프.
//   a_to_b == true  → A 에서 읽어 B 로 쓰기. MATCH_SUMMARY 는 가로챔.
//   a_to_b == false → B → A.
//
// MATCH_SUMMARY 는 반드시 ranked + meta 연동 + 양쪽 player_id != 0 일 때만
// 가로챈다. 그 외의 경우(unranked / no meta)는 투명 포워딩.
void forwarderLoop(std::shared_ptr<Channel> ch, bool a_to_b)
{
    const net::TcpSocket& from = a_to_b ? ch->A : ch->B;
    const net::TcpSocket& to   = a_to_b ? ch->B : ch->A;
    const char*           dir  = a_to_b ? "A->B" : "B->A";

    const bool rankedMatch = (ch->meta != nullptr) &&
                             (ch->playerA_id != 0) &&
                             (ch->playerB_id != 0);

    // parse_frames 는 스트림 버퍼가 필요. MATCH_SUMMARY 만 따로 빼내고 나머지는
    // 원본 바이트 그대로 to 에 보내야 한다 — 이를 위해 raw 와 parsed 두 경로를
    // 유지한다. rankedMatch=false 면 파싱하지 않고 raw 를 그대로 전달.
    std::vector<uint8_t> raw; raw.reserve(4096);
    std::vector<uint8_t> streamBuf; streamBuf.reserve(4096);

    // Lobby 에서 남긴 prefix 바이트가 있으면 첫 iteration 의 raw 로 사용한다.
    //   · unranked 모드: 그대로 to 로 송신.
    //   · ranked 모드: streamBuf 에 들어가 프레이밍 파서가 처리.
    bool havePrefix = false;
    {
        std::vector<uint8_t>& pref = a_to_b ? ch->prefixFromA : ch->prefixFromB;
        if (!pref.empty()) {
            raw = std::move(pref);
            pref.clear();
            havePrefix = true;
        }
    }

    while (!ch->closed.load()) {
        if (havePrefix) {
            havePrefix = false;  // raw 는 이미 준비돼 있음 — 바로 처리.
        } else {
            raw.clear();
            if (!net::tcp_recv_some(from, raw)) break;
            if (raw.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        if (!rankedMatch) {
            // 투명 포워딩 — MATCH_SUMMARY 도 통과 (클라는 서버 응답 못 받아도 OK).
            // sendMuA/B 로 보호해 finalizeRanked / 반대 방향 forwarder 와 직렬화.
            const bool ok = a_to_b ? sendToB(*ch, raw.data(), raw.size())
                                   : sendToA(*ch, raw.data(), raw.size());
            if (!ok) break;
            continue;
        }

        // ranked: 프레임 단위로 파싱해 MATCH_SUMMARY 를 가로챈다.
        // parse_frames 는 streamBuf 를 소비형으로 다룸 (완성된 프레임만큼 앞에서 제거).
        streamBuf.insert(streamBuf.end(), raw.begin(), raw.end());
        // 프레임 경계를 파악하기 위해 build_frame 의 역함수가 필요. 우리 프레임
        // 포맷은 [LEN:2][TYPE:1][PAYLOAD:LEN-1][CHK:4] — LEN 앞 2바이트로 총
        // 바이트 수 (= LEN + 2 + 4) 를 알 수 있다. parse_frames 는 체크섬까지
        // 확인해 프레임 객체를 주지만, raw 바이트는 소비하고 버린다. 그래서
        // MATCH_SUMMARY 가 아닌 프레임은 원본을 다시 재조립해 to 로 보내야 한다.
        //
        // 간단하게 가기 위해 우리는 streamBuf 를 직접 프레이밍한다:
        //   · LEN 을 읽어 완성된 프레임이 있으면 (2+len+4 bytes) 잘라낸다.
        //   · TYPE 이 MATCH_SUMMARY 이면 수집만 하고 포워딩하지 않는다.
        //   · 그 외 TYPE 이면 잘라낸 바이트 전체를 to 로 송신.
        //
        // MATCH_SUMMARY 는 relay 가 실제로 신뢰해 ELO 갱신에 쓰므로, 이 타입만큼은
        // 최소한 framing.cpp 와 동일한 payload checksum 을 재검증한다. 나머지
        // 프레임은 기존처럼 투명 포워딩한다.

        bool sendFailed = false;
        while (streamBuf.size() >= 2) {
            const uint16_t payloadAndType = static_cast<uint16_t>(streamBuf[0]) |
                                            (static_cast<uint16_t>(streamBuf[1]) << 8);
            const size_t totalNeeded = 2u + payloadAndType + 4u;  // LEN(2)+LEN+CHK(4)
            if (streamBuf.size() < totalNeeded) break;

            if (payloadAndType < 1u) {
                std::cerr << "[relay] match=" << ch->match_id
                          << " dropping malformed frame (len=0)\n";
                streamBuf.erase(streamBuf.begin(), streamBuf.begin() + totalNeeded);
                continue;
            }

            const uint8_t typeByte = streamBuf[2];
            if (typeByte == static_cast<uint8_t>(net::MsgType::MATCH_SUMMARY)) {
                // 페이로드는 [2..2+len-1], len-1 은 payload 길이 (TYPE 제외).
                const size_t payloadLen = payloadAndType >= 1u ? payloadAndType - 1u : 0u;
                const uint8_t* payloadPtr = streamBuf.data() + 3;
                const uint32_t chk = net::le_read_u32(streamBuf.data() + 2u + payloadAndType);
                const uint32_t calc = payloadLen == 0
                    ? 0u
                    : net::fnv1a32(payloadPtr, payloadLen);

                if (chk != calc) {
                    std::cerr << "[relay] match=" << ch->match_id
                              << " dropping MATCH_SUMMARY with bad checksum from "
                              << (a_to_b ? "A" : "B") << "\n";
                    streamBuf.erase(streamBuf.begin(), streamBuf.begin() + totalNeeded);
                    continue;
                }

                std::vector<uint8_t> payload(streamBuf.begin() + 3,
                                             streamBuf.begin() + 3 + payloadLen);
                Summary s{};
                if (parse_summary(payload, s)) {
                    {
                        std::lock_guard<std::mutex> lk(ch->sumMu);
                        if (a_to_b) { if (!ch->summaryA) ch->summaryA = s; }
                        else        { if (!ch->summaryB) ch->summaryB = s; }
                    }
                    std::cerr << "[relay] match=" << ch->match_id
                              << " got MATCH_SUMMARY from " << (a_to_b ? "A" : "B")
                              << " won=" << (int)s.won
                              << " score=" << s.my_score
                              << "\n";
                } else {
                    std::cerr << "[relay] match=" << ch->match_id
                              << " dropping malformed MATCH_SUMMARY payload from "
                              << (a_to_b ? "A" : "B")
                              << " size=" << payload.size() << "\n";
                }
                // 가로챔 — 상대 포워딩 안 함.
            } else {
                // 다른 프레임은 원본 바이트 그대로 to 로 송신 (sendMuA/B 로 직렬화).
                const bool ok = a_to_b ? sendToB(*ch, streamBuf.data(), totalNeeded)
                                       : sendToA(*ch, streamBuf.data(), totalNeeded);
                if (!ok) {
                    sendFailed = true;
                    break;
                }
            }
            streamBuf.erase(streamBuf.begin(), streamBuf.begin() + totalNeeded);
        }

        if (sendFailed) break;

        // 양쪽 MATCH_SUMMARY 모두 모였다면 finalize. (매 루프 체크 — 가벼움)
        bool both = false;
        {
            std::lock_guard<std::mutex> lk(ch->sumMu);
            both = ch->summaryA.has_value() && ch->summaryB.has_value() && !ch->summaryHandled;
        }
        if (both) {
            finalizeRanked(*ch);
        }
    }

    std::cerr << "[relay] match=" << ch->match_id << " " << dir << " end\n";
    ch->closed.store(true);

    // 연결 종료 직전 — 한쪽만 summary 보내고 끊긴 경우에도 상대에겐 delta=0 을
    // 돌려주고 싶지만, 복잡도 대비 이득이 작으므로 skip. finalize 는 "양쪽 모두
    // 도착했을 때" 만 호출됨.

    if (--ch->forwarder_count == 0) {
        net::tcp_close(ch->A);
        net::tcp_close(ch->B);
        std::cerr << "[relay] match=" << ch->match_id << " closed\n";
    }
}

// MATCH_FOUND 프레임 전송. 페이로드: [role:1][seed:8 LE]
bool sendMatchFound(const net::TcpSocket& sock, uint8_t role, uint64_t seed) {
    std::vector<uint8_t> payload;
    payload.reserve(9);
    payload.push_back(role);
    net::le_write_u64(payload, seed);
    auto frame = net::build_frame(net::MsgType::MATCH_FOUND, payload);
    return net::tcp_send_all(sock, frame.data(), frame.size());
}

// 포워더 채널을 열어 detached 스레드 2개로 양방향 바이트 포워딩 시작.
// MATCH_FOUND 는 이미 호출자가 송신했다고 가정.
// prefixFromA/B: lobby 에서 이미 recv 했지만 forwarder 로 넘겨야 할 raw 바이트.
//   (READY 교환 직후 상대가 바로 PING/INPUT 을 보내 lobby 가 그 바이트를 kernel 에서
//    끌어왔을 때, 이 상태를 잃지 않도록 한다.)
void startForwardingWithPrefix(Match match, meta::client::MetaClient* meta,
                                std::vector<uint8_t> prefixFromA,
                                std::vector<uint8_t> prefixFromB) {
    std::cerr << "[relay] match forwarding id=" << match.match_id
              << " HOST=conn" << match.a.conn_id
              << " (pid=" << match.a.player_id << " elo=" << match.a.elo << ")"
              << " GUEST=conn" << match.b.conn_id
              << " (pid=" << match.b.player_id << " elo=" << match.b.elo << ")"
              << " seed=0x" << std::hex << match.seed << std::dec << "\n";

    auto ch = std::make_shared<Channel>();
    ch->A           = match.a.sock;
    ch->B           = match.b.sock;
    ch->match_id    = match.match_id;
    ch->playerA_id  = match.a.player_id;
    ch->playerB_id  = match.b.player_id;
    ch->playerA_elo = match.a.elo;
    ch->playerB_elo = match.b.elo;
    ch->meta        = meta;
    ch->prefixFromA = std::move(prefixFromA);
    ch->prefixFromB = std::move(prefixFromB);

    std::thread(forwarderLoop, ch, true ).detach();
    std::thread(forwarderLoop, ch, false).detach();
}

void startForwarding(Match match, meta::client::MetaClient* meta) {
    startForwardingWithPrefix(std::move(match), meta, {}, {});
}

// 랜덤 큐 전용: MATCH_FOUND 이후 양쪽 READY(1) 확인까지 대기하는 로비 루프.
// detached thread 로 돌아 matcher 를 블록하지 않는다.
//
// 규칙:
//   · READY(1) 두 번 모두 수신 → startForwarding.
//   · READY(0) / QUEUE_CANCEL / EOF / send 실패 / 30s 타임아웃 → 양측 close.
//   · 수락 상태는 상대에게 그대로 forward — 클라 UI 에서 "peer ready" 표시용.
//
// 주의 — 프레임 소비 정책:
//   클라이언트는 상대 READY(1) 포워딩을 본 순간 ioThread 로 전환해 곧바로 PING 등
//   게임 프레임을 송신할 수 있다. 그 바이트는 아직 forwarder 가 recv 하기 전 lobby
//   스레드 차원의 TCP 버퍼에 쌓일 수 있으므로, 이 함수는 parse_frames(전체 소비)
//   대신 "한 프레임씩 앞에서 파싱" 방식으로 동작한다. READY/QUEUE_CANCEL 은 직접
//   처리하고, 그 외 타입을 만나면 더 파싱하지 않고 멈춰 나머지 바이트를
//   Channel::prefixFromA / prefixFromB 로 forwarder 에게 이관한다.
void queueLobbyThread(Match match, meta::client::MetaClient* meta) {
    constexpr auto kConfirmTimeout = std::chrono::seconds(30);
    constexpr auto kPollInterval   = std::chrono::milliseconds(10);
    constexpr size_t LEN_FIELD      = 2;
    constexpr size_t TYPE_FIELD     = 1;
    constexpr size_t CHECKSUM_FIELD = 4;

    bool aReady = false;
    bool bReady = false;
    bool abort  = false;

    std::vector<uint8_t> bufA; bufA.reserve(64);
    std::vector<uint8_t> bufB; bufB.reserve(64);

    const auto deadline = std::chrono::steady_clock::now() + kConfirmTimeout;

    auto forward_ready = [](const net::TcpSocket& dst, uint8_t ready) -> bool {
        std::vector<uint8_t> pl; pl.push_back(ready ? 1 : 0);
        auto fr = net::build_frame(net::MsgType::READY, pl);
        return net::tcp_send_all(dst, fr.data(), fr.size());
    };

    // "한 프레임씩 처리" — READY/QUEUE_CANCEL 은 소비하고 action 실행.
    // 그 외 타입(게임 프레임)을 만나면 즉시 멈춰 버퍼의 현재 상태를 그대로 보존한다.
    // 반환값: 0=진행 계속, 1=이 사이드 ready 확정, 2=이 사이드 decline/cancel, -1=send 실패.
    auto consume_ready_frames = [&](std::vector<uint8_t>& buf,
                                     const net::TcpSocket& peer) -> int {
        while (buf.size() >= LEN_FIELD + CHECKSUM_FIELD) {
            const uint16_t len = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
            const size_t totalNeeded = LEN_FIELD + (size_t)len + CHECKSUM_FIELD;
            if (buf.size() < totalNeeded) return 0;  // 미완성 — 다음 recv 대기.

            // 손상 프레임(len=0) — 한 프레임치 바이트를 버리고 계속.
            if (len < TYPE_FIELD) {
                buf.erase(buf.begin(), buf.begin() + totalNeeded);
                continue;
            }

            const uint8_t type = buf[LEN_FIELD];
            // 게임 프레임 (READY / QUEUE_CANCEL 이 아닌 것) 을 보면 멈춘다 — 포워더로 이관.
            if (type != (uint8_t)net::MsgType::READY &&
                type != (uint8_t)net::MsgType::QUEUE_CANCEL) {
                return 0;
            }

            // 체크섬 검증 (다른 invalid 프레임이면 버리고 계속).
            const size_t payloadLen = (size_t)len - TYPE_FIELD;
            const uint32_t chk = net::le_read_u32(buf.data() + LEN_FIELD + (size_t)len);
            const uint32_t calc = payloadLen == 0 ? 0u
                : net::fnv1a32(buf.data() + LEN_FIELD + TYPE_FIELD, payloadLen);
            if (chk != calc) {
                buf.erase(buf.begin(), buf.begin() + totalNeeded);
                continue;
            }

            if (type == (uint8_t)net::MsgType::READY) {
                const uint8_t v = payloadLen == 0 ? 0 : buf[LEN_FIELD + TYPE_FIELD];
                buf.erase(buf.begin(), buf.begin() + totalNeeded);
                if (v == 0) {
                    forward_ready(peer, 0);
                    return 2;
                }
                if (!forward_ready(peer, 1)) return -1;
                return 1;
            }
            // QUEUE_CANCEL
            buf.erase(buf.begin(), buf.begin() + totalNeeded);
            forward_ready(peer, 0);
            return 2;
        }
        return 0;
    };

    while (!abort && !(aReady && bReady)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            std::cerr << "[relay] match=" << match.match_id
                      << " queue lobby timeout (aReady=" << aReady
                      << " bReady=" << bReady << ")\n";
            abort = true;
            break;
        }

        // 양쪽 소켓 모두 폴링해 EOF 를 감지한다 — ready 확정된 쪽이 이후에 창 닫기
        // 같은 이유로 끊어져도 상대에게 즉시 알려 "상대가 계속 있는 것처럼 보이는"
        // 버그를 방지. 단, 프레임 파싱(READY/QUEUE_CANCEL 소비) 은 아직 ready 가
        // 안 된 쪽만. ready 확정 뒤의 raw 바이트는 bufA/bufB 에 그대로 쌓여 나중에
        // forwarder 로 prefix 이관된다.
        const bool okA = net::tcp_recv_some(match.a.sock, bufA);
        const bool okB = net::tcp_recv_some(match.b.sock, bufB);
        if (!okA) {
            std::cerr << "[relay] match=" << match.match_id
                      << " queue lobby A disconnected (aReady=" << aReady
                      << " bReady=" << bReady << ")\n";
            // 상대에게 READY(0) 전송해 "상대 취소" 시그널 — 소켓이 이미 닫혔을
            // 수 있지만 send 실패해도 어차피 다음 라인에서 close.
            if (bReady || !aReady) forward_ready(match.b.sock, 0);
            abort = true; break;
        }
        if (!okB) {
            std::cerr << "[relay] match=" << match.match_id
                      << " queue lobby B disconnected (aReady=" << aReady
                      << " bReady=" << bReady << ")\n";
            if (aReady || !bReady) forward_ready(match.a.sock, 0);
            abort = true; break;
        }

        if (!aReady) {
            int r = consume_ready_frames(bufA, match.b.sock);
            if (r == 1) {
                aReady = true;
            } else if (r == 2) {
                std::cerr << "[relay] match=" << match.match_id
                          << " A declined/cancelled in lobby\n";
                abort = true; break;
            } else if (r == -1) {
                abort = true; break;
            }
        }
        if (abort) break;

        if (!bReady) {
            int r = consume_ready_frames(bufB, match.a.sock);
            if (r == 1) {
                bReady = true;
            } else if (r == 2) {
                std::cerr << "[relay] match=" << match.match_id
                          << " B declined/cancelled in lobby\n";
                abort = true; break;
            } else if (r == -1) {
                abort = true; break;
            }
        }
        if (abort) break;

        if (!(aReady && bReady)) {
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    if (abort) {
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    std::cerr << "[relay] match=" << match.match_id
              << " queue lobby accepted, starting forwarders\n";

    // lobby 에서 남긴 raw 바이트(READY 이후 도착한 게임 프레임) 를 forwarder 로 이관.
    startForwardingWithPrefix(std::move(match), meta, std::move(bufA), std::move(bufB));
}

}  // namespace

void startPump(Match match, meta::client::MetaClient* meta) {
    constexpr uint8_t ROLE_HOST  = 1;
    constexpr uint8_t ROLE_GUEST = 2;

    const bool ok_a = sendMatchFound(match.a.sock, ROLE_HOST,  match.seed);
    const bool ok_b = sendMatchFound(match.b.sock, ROLE_GUEST, match.seed);

    if (!ok_a || !ok_b) {
        std::cerr << "[relay] MATCH_FOUND send failed, match=" << match.match_id << "\n";
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    startForwarding(std::move(match), meta);
}

void startQueuePump(Match match, meta::client::MetaClient* meta) {
    constexpr uint8_t ROLE_HOST  = 1;
    constexpr uint8_t ROLE_GUEST = 2;

    const bool ok_a = sendMatchFound(match.a.sock, ROLE_HOST,  match.seed);
    const bool ok_b = sendMatchFound(match.b.sock, ROLE_GUEST, match.seed);

    if (!ok_a || !ok_b) {
        std::cerr << "[relay] MATCH_FOUND send failed, match=" << match.match_id << "\n";
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    // matcher 스레드를 블록하지 않도록 로비 대기 루프는 detached thread 에서.
    std::thread(queueLobbyThread, std::move(match), meta).detach();
}

}  // namespace relay
