#include "relay.h"
#include "worker_group.h"

#include "../net/framing.h"
#include "../net/socket.h"
#include "../meta/http_client.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

namespace {

std::atomic<bool> s_stopping{false};
// Lobby and forwarder threads have a separate bound from handshake workers.
constexpr size_t kMaxRelayWorkers = 512;
WorkerGroup s_workers{"relay", kMaxRelayWorkers};

// MATCH_SUMMARY is a fixed 21-byte payload; see net/framing.h.
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

// State shared by both forwarding directions.
struct Channel {
    net::TcpSocket   A;            // HOST 소켓
    net::TcpSocket   B;            // GUEST 소켓
    uint32_t         match_id{0};
    std::string      match_uuid;

    int64_t          playerA_id{0};
    int64_t          playerB_id{0};
    int              playerA_elo{0};
    int              playerB_elo{0};
    std::shared_ptr<PlayerSessionLease> playerA_session;
    std::shared_ptr<PlayerSessionLease> playerB_session;

    std::atomic<bool> closed{false};
    std::atomic<int>  forwarder_count{2};
    std::atomic<int>  disconnect_side{0}; // 1=A, 2=B; first observed failure wins

    // MATCH_SUMMARY 수집
    std::mutex              sumMu;
    std::optional<Summary>  summaryA;
    std::optional<Summary>  summaryB;
    bool                    summaryHandled{false};   // 한 번만 처리

    // Bytes read by the lobby after READY are handed to the forwarders.
    std::vector<uint8_t>   prefixFromA;
    std::vector<uint8_t>   prefixFromB;

    // Serialize partial sends to each destination socket.
    std::mutex             sendMuA;
    std::mutex             sendMuB;

    // meta 호출 경로. nullptr이면 unranked raw 전달이며 MATCH_SUMMARY도
    // 서버 결과로 해석하지 않는다.
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

// Cross-check both summaries and publish one ranked result.
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

    // Both sides must agree on winner, score, and lines.
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

    // A mismatch is stored as a draw and does not change RP.
    const int      duration_s = static_cast<int>(std::max(a.duration_s, b.duration_s));
    const int      score_a    = static_cast<int>(a.my_score);
    const int      score_b    = static_cast<int>(b.my_score);
    const int      lines_a    = static_cast<int>(a.my_lines);
    const int      lines_b    = static_cast<int>(b.my_lines);

    int deltaA = 0, deltaB = 0;
    int eloABefore = ch.playerA_elo, eloAAfter = ch.playerA_elo;
    int eloBBefore = ch.playerB_elo, eloBAfter = ch.playerB_elo;

    if (ch.meta) {
        auto res = ch.meta->post_match(ch.match_uuid, ch.playerA_id, ch.playerB_id, winner,
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

// A peer lost before finalizeRanked ran. 몰수패 처리 진입점 — 요약이 몇 개
// 수집됐는지에 따라 세 갈래로 나뉜다 (각 분기 주석 참고).
// disconnectSide 는 "먼저 끊긴 쪽"(1=A, 2=B, 0=미상)일 뿐 패자가 아니다 —
// 승자 판정에는 쓰지 않고 MATCH_RESULT 송신 대상(생존자) 선정에만 쓴다.
void finalizeForfeit(Channel& ch, int disconnectSide)
{
    if (!ch.meta || ch.playerA_id == 0 || ch.playerB_id == 0) return;

    Summary a{}, b{};
    bool haveA = false;
    bool haveB = false;
    {
        std::lock_guard<std::mutex> lk(ch.sumMu);
        if (ch.summaryHandled) return;
        haveA = ch.summaryA.has_value();
        haveB = ch.summaryB.has_value();
        // 양쪽 요약이 다 모였으면 몰수패가 아니다 — summaryHandled 를 여기서
        // 선점하지 않고 finalizeRanked 의 교차검증 경로에 맡긴다 (락 밖에서 위임).
        if (!(haveA && haveB)) {
            ch.summaryHandled = true;
        }
        if (haveA) a = *ch.summaryA;
        if (haveB) b = *ch.summaryB;
    }

    // (1) 양쪽 요약 존재: 회선이 끊겼어도 경기 자체는 완주된 것이다 (예: 승패
    //     확정 직후 요약만 보내고 즉시 종료). 교차검증으로 승자를 확정한다.
    if (haveA && haveB) {
        finalizeRanked(ch);
        return;
    }

    // (2) 요약이 하나도 없음(즉시 이탈, 무경기): meta 에 post_match 를 보내지
    //     않는다 — RP 미반영. 커스텀 룸에서 READY 직후 끊기를 반복하는 담합
    //     RP 파밍과, 동시 단절 시 disconnect_side 관측 순서 하나로 임의 승자를
    //     만들어 RP 를 오염시키는 것을 함께 막는다. 생존자에게는 델타 0 의
    //     MATCH_RESULT 를 보내 결과 대기 화면에서 빠져나오게 한다.
    if (!haveA && !haveB) {
        auto frA = build_match_result(ch.playerA_elo, ch.playerA_elo, 0);
        auto frB = build_match_result(ch.playerB_elo, ch.playerB_elo, 0);
        // disconnectSide==0(순서 미상)이면 양쪽 다 시도 — 죽은 소켓으로의
        // send 는 무해하게 실패한다.
        if (disconnectSide != 1) sendToA(ch, frA);
        if (disconnectSide != 2) sendToB(ch, frB);
        std::cerr << "[relay] match=" << ch.match_id
                  << " no summaries -> no meta post (delta=0)\n";
        return;
    }

    // (3) 한쪽 요약만 존재: 그 요약의 won 플래그를 존중해 승자를 정한다.
    //     종전에는 disconnectSide 를 무조건 패자로 기록했는데, 그러면 이긴 쪽이
    //     승리 요약을 제출한 직후 회선이 끊겼을 때 제출된 승리 요약이 무시되고
    //     승자가 패자로 뒤집히는 버그가 있었다.
    int64_t winner = 0;
    if (haveA) winner = a.won ? ch.playerA_id : ch.playerB_id;
    else       winner = b.won ? ch.playerB_id : ch.playerA_id;

    const int scoreA = static_cast<int>(haveA ? a.my_score : b.opp_score);
    const int scoreB = static_cast<int>(haveB ? b.my_score : a.opp_score);
    const int linesA = static_cast<int>(haveA ? a.my_lines : b.opp_lines);
    const int linesB = static_cast<int>(haveB ? b.my_lines : a.opp_lines);
    const int duration = static_cast<int>(std::max(a.duration_s, b.duration_s));

    auto res = ch.meta->post_match(ch.match_uuid, ch.playerA_id, ch.playerB_id,
                                   winner, scoreA, scoreB, linesA, linesB,
                                   duration, 3);
    if (!res) {
        std::cerr << "[relay] match=" << ch.match_id
                  << " forfeit meta POST failed\n";
        return;
    }

    auto frA = build_match_result(res->a.elo_before, res->a.elo_after, res->a.delta);
    auto frB = build_match_result(res->b.elo_before, res->b.elo_after, res->b.delta);
    // 끊긴 쪽 소켓은 이미 죽어 있으므로 생존 가능성이 있는 쪽에만 보낸다.
    if (disconnectSide != 1) sendToA(ch, frA);
    if (disconnectSide != 2) sendToB(ch, frB);
    std::cerr << "[relay] match=" << ch.match_id << " forfeit winner="
              << (winner == ch.playerA_id ? "A" : "B")
              << " (summary from " << (haveA ? "A" : "B")
              << ", disconnect=" << disconnectSide << ") saved meta match="
              << res->match_id << "\n";
}

// 한 방향 포워딩 루프.
//   a_to_b == true  → A 에서 읽어 B 로 쓰기. MATCH_SUMMARY 는 가로챔.
//   a_to_b == false → B → A.
//
// MATCH_SUMMARY는 ranked + meta 연동 + 양쪽 player_id != 0일 때만 가로챈다.
// unranked/no-meta 경로는 프레임 경계도 만들지 않고 받은 byte를 그대로 전달한다.
void forwarderLoop(std::shared_ptr<Channel> ch, bool a_to_b)
{
    const net::TcpSocket& from = a_to_b ? ch->A : ch->B;
    const net::TcpSocket& to   = a_to_b ? ch->B : ch->A;
    const char*           dir  = a_to_b ? "A->B" : "B->A";
    int disconnectSide = 0;

    // Every exit stops the peer direction and releases the channel once.
    struct ForwarderCompletion {
        std::shared_ptr<Channel> channel;
        const char* direction;
        int* failureSide;

        ~ForwarderCompletion()
        {
            std::cerr << "[relay] match=" << channel->match_id
                      << " " << direction << " end\n";
            if (*failureSide != 0 && !s_stopping.load()) {
                int expected = 0;
                channel->disconnect_side.compare_exchange_strong(expected, *failureSide);
            }
            channel->closed.store(true);
            if (--channel->forwarder_count == 0) {
                if (!s_stopping.load()) {
                    finalizeForfeit(*channel, channel->disconnect_side.load());
                }
                net::tcp_close(channel->A);
                net::tcp_close(channel->B);
                std::cerr << "[relay] match=" << channel->match_id << " closed\n";
            }
        }
    } completion{ch, dir, &disconnectSide};

    const bool rankedMatch = (ch->meta != nullptr) &&
                             (ch->playerA_id != 0) &&
                             (ch->playerB_id != 0);

    // Ranked mode intercepts summaries; unranked mode forwards raw bytes.
    std::vector<uint8_t> raw; raw.reserve(4096);
    std::vector<uint8_t> streamBuf; streamBuf.reserve(4096);

    // Consume lobby-prefetched bytes before reading the socket.
    bool havePrefix = false;
    auto lastActivity = std::chrono::steady_clock::now();
    auto byteWindowStart = lastActivity;
    size_t byteWindow = 0;
    constexpr auto kIdleTimeout = std::chrono::seconds(15);
    constexpr size_t kMaxBytesPerSecond = 64 * 1024;
    {
        std::vector<uint8_t>& pref = a_to_b ? ch->prefixFromA : ch->prefixFromB;
        if (!pref.empty()) {
            raw = std::move(pref);
            pref.clear();
            havePrefix = true;
        }
    }

    while (!ch->closed.load() && !s_stopping.load()) {
        if (havePrefix) {
            havePrefix = false;  // raw 는 이미 준비돼 있음 — 바로 처리.
        } else {
            raw.clear();
            if (!net::tcp_recv_some(from, raw)) {
                disconnectSide = a_to_b ? 1 : 2;
                break;
            }
            if (raw.empty()) {
                if (std::chrono::steady_clock::now() - lastActivity >= kIdleTimeout) {
                    disconnectSide = a_to_b ? 1 : 2;
                    std::cerr << "[relay] match=" << ch->match_id << " " << dir
                              << " idle timeout\n";
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        lastActivity = now;
        if (now - byteWindowStart >= std::chrono::seconds(1)) {
            byteWindowStart = now;
            byteWindow = 0;
        }
        byteWindow += raw.size();
        if (byteWindow > kMaxBytesPerSecond) {
            disconnectSide = a_to_b ? 1 : 2;
            std::cerr << "[relay] match=" << ch->match_id << " " << dir
                      << " byte rate exceeded\n";
            break;
        }

        if (!rankedMatch) {
            // Unranked raw 전달 — MATCH_SUMMARY도 평범한 wire byte일 뿐이며
            // MATCH_RESULT는 만들지 않는다. sendMuA/B로 보호해 반대 방향
            // forwarder와 같은 destination socket에 쓰는 순서를 직렬화한다.
            const bool ok = a_to_b ? sendToB(*ch, raw.data(), raw.size())
                                   : sendToA(*ch, raw.data(), raw.size());
            if (!ok) {
                disconnectSide = a_to_b ? 2 : 1;
                break;
            }
            continue;
        }

        // Ranked mode parses frame boundaries to intercept MATCH_SUMMARY.
        streamBuf.insert(streamBuf.end(), raw.begin(), raw.end());
        // Non-summary frames retain their original wire bytes.

        bool sendFailed = false;
        while (streamBuf.size() >= 2) {
            const uint16_t payloadAndType = static_cast<uint16_t>(streamBuf[0]) |
                                            (static_cast<uint16_t>(streamBuf[1]) << 8);

            // Reject oversized declarations before the buffer grows.
            // 상한은 net/framing.h 가 공개하는 값을 직접 참조 — 로컬 사본이
            // framing 구현과 어긋나는 drift 를 막는다.
            if (static_cast<size_t>(payloadAndType) > net::kMaxPayloadBytes + 1u) {
                std::cerr << "[relay] match=" << ch->match_id
                          << " dropping over-sized frame (len=" << payloadAndType
                          << ") from " << (a_to_b ? "A" : "B") << "\n";
                streamBuf.clear();
                break;
            }

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
                    disconnectSide = a_to_b ? 2 : 1;
                    sendFailed = true;
                    break;
                }
            }
            streamBuf.erase(streamBuf.begin(), streamBuf.begin() + totalNeeded);
        }

        // 양쪽 MATCH_SUMMARY 모두 모였다면 finalize. (매 루프 체크 — 가벼움)
        // sendFailed 로 빠져나가기 "직전"에도 반드시 수행한다 — 양 방향이 거의
        // 동시에 send 실패로 죽는 타이밍에는, 이 배치에서 마지막 요약을 방금
        // 가로챘는데도 어느 쪽도 루프를 한 바퀴 더 돌지 못해 교차검증이 생략되고
        // forfeit 경로로 흘러가는 경합이 있었다. (finalizeForfeit 도 양쪽 요약이
        // 있으면 위임하지만, 소켓이 닫히기 전에 결과를 보내려면 여기가 먼저다.)
        bool both = false;
        {
            std::lock_guard<std::mutex> lk(ch->sumMu);
            both = ch->summaryA.has_value() && ch->summaryB.has_value() && !ch->summaryHandled;
        }
        if (both) {
            finalizeRanked(*ch);
        }

        if (sendFailed) break;
    }

}

// MATCH_FOUND 프레임 전송.
// 페이로드: [role:1][seed:8 LE][my_icon_len:1][my_icon:N]
//             [peer_icon_len:1][peer_icon:N][uuid_len:1][match_uuid:N]
// 뒤쪽 icon/UUID 필드는 구버전 클라와의 완만한 호환을 위해 optional 처럼 파싱한다.
bool sendMatchFound(const net::TcpSocket& sock, uint8_t role, uint64_t seed,
                    const std::string& my_icon,
                    const std::string& peer_icon,
                    const std::string& match_uuid) {
    const std::string my = my_icon.empty() ? "default" : my_icon;
    const std::string peer = peer_icon.empty() ? "default" : peer_icon;
    const size_t my_len = std::min<size_t>(my.size(), 255);
    const size_t peer_len = std::min<size_t>(peer.size(), 255);

    std::vector<uint8_t> payload;
    const size_t uuid_len = std::min<size_t>(match_uuid.size(), 255);
    payload.reserve(9 + 1 + my_len + 1 + peer_len + 1 + uuid_len);
    payload.push_back(role);
    net::le_write_u64(payload, seed);
    auto append_icon = [&](const std::string& icon, size_t n) {
        payload.push_back(static_cast<uint8_t>(n));
        const size_t old_size = payload.size();
        payload.resize(old_size + n);
        if (n > 0) {
            std::memcpy(payload.data() + old_size, icon.data(), n);
        }
    };
    append_icon(my, my_len);
    append_icon(peer, peer_len);
    payload.push_back(static_cast<uint8_t>(uuid_len));
    payload.insert(payload.end(), match_uuid.begin(), match_uuid.begin() + uuid_len);
    auto frame = net::build_frame(net::MsgType::MATCH_FOUND, payload);
    return net::tcp_send_all(sock, frame.data(), frame.size());
}

// Start both forwarding directions after MATCH_FOUND has been sent.
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
    ch->match_uuid  = match.match_uuid;
    ch->playerA_id  = match.a.player_id;
    ch->playerB_id  = match.b.player_id;
    ch->playerA_elo = match.a.elo;
    ch->playerB_elo = match.b.elo;
    ch->playerA_session = std::move(match.a.session_lease);
    ch->playerB_session = std::move(match.b.session_lease);
    ch->meta        = meta;
    ch->prefixFromA = std::move(prefixFromA);
    ch->prefixFromB = std::move(prefixFromB);

    const bool launchedA = s_workers.launch([ch] { forwarderLoop(ch, true); });
    const bool launchedB = s_workers.launch([ch] { forwarderLoop(ch, false); });
    if (!launchedA || !launchedB) {
        ch->closed.store(true);
        net::tcp_close(ch->A);
        net::tcp_close(ch->B);
    }
}

void startForwarding(Match match, meta::client::MetaClient* meta) {
    startForwardingWithPrefix(std::move(match), meta, {}, {});
}

// Queue lobby forwards READY state and preserves early game frames.
void queueLobbyThread(Match match, meta::client::MetaClient* meta) {
    constexpr auto kConfirmTimeout = std::chrono::seconds(30);
    constexpr auto kPollInterval   = std::chrono::milliseconds(10);
    constexpr size_t LEN_FIELD          = 2;
    constexpr size_t TYPE_FIELD         = 1;
    constexpr size_t CHECKSUM_FIELD     = 4;
    // 페이로드 상한은 net::kMaxPayloadBytes (framing.h) 를 직접 참조한다.
    // Bound bytes received between READY and forwarder ownership.
    constexpr size_t kMaxLobbyBufBytes  = 64 * 1024;

    bool aReady = false;
    bool bReady = false;
    bool abort  = false;

    // Continue from bytes already read during matchmaking.
    std::vector<uint8_t> bufA = std::move(match.a.streamBuf);
    std::vector<uint8_t> bufB = std::move(match.b.streamBuf);

    const auto deadline = std::chrono::steady_clock::now() + kConfirmTimeout;

    auto forward_ready = [](const net::TcpSocket& dst, uint8_t ready) -> bool {
        std::vector<uint8_t> pl; pl.push_back(ready ? 1 : 0);
        auto fr = net::build_frame(net::MsgType::READY, pl);
        return net::tcp_send_all(dst, fr.data(), fr.size());
    };

    // Return 0=continue, 1=ready, 2=decline, -1=send failure.
    auto consume_ready_frames = [&](std::vector<uint8_t>& buf,
                                     const net::TcpSocket& peer) -> int {
        while (buf.size() >= LEN_FIELD + CHECKSUM_FIELD) {
            const uint16_t len = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);

            // 페이로드 상한 초과 — framing.cpp::parse_frames 와 동일하게
            // 스트림 전체를 버리고 ready/cancel 어느 것도 소비하지 않는다.
            // 호출자는 이 사이드를 abort 처리한다.
            if ((size_t)len > net::kMaxPayloadBytes + TYPE_FIELD) {
                buf.clear();
                return -1;
            }

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

    while (!abort && !(aReady && bReady) && !s_stopping.load()) {
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

        // 로비 버퍼 상한 — ready 확정 뒤 forwarder 이관 대기 중인 raw 바이트가
        // 무한정 쌓이는 것을 차단. 초과하는 쪽은 프로토콜을 벗어난 것으로 보고
        // 매치를 중단한다.
        if (bufA.size() > kMaxLobbyBufBytes || bufB.size() > kMaxLobbyBufBytes) {
            std::cerr << "[relay] match=" << match.match_id
                      << " queue lobby buffer overflow (A=" << bufA.size()
                      << " B=" << bufB.size() << ") -> abort\n";
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

    if (abort || s_stopping.load()) {
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

    if (s_stopping.load()) {
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    const bool ok_a = sendMatchFound(match.a.sock, ROLE_HOST,  match.seed,
                                     match.a.selected_icon_id, match.b.selected_icon_id,
                                     match.match_uuid);
    const bool ok_b = sendMatchFound(match.b.sock, ROLE_GUEST, match.seed,
                                     match.b.selected_icon_id, match.a.selected_icon_id,
                                     match.match_uuid);

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

    if (s_stopping.load()) {
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    const bool ok_a = sendMatchFound(match.a.sock, ROLE_HOST,  match.seed,
                                     match.a.selected_icon_id, match.b.selected_icon_id,
                                     match.match_uuid);
    const bool ok_b = sendMatchFound(match.b.sock, ROLE_GUEST, match.seed,
                                     match.b.selected_icon_id, match.a.selected_icon_id,
                                     match.match_uuid);

    if (!ok_a || !ok_b) {
        std::cerr << "[relay] MATCH_FOUND send failed, match=" << match.match_id << "\n";
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    // matcher 스레드를 블록하지 않되 종료 시 server가 drain할 수 있게 추적한다.
    auto pending = std::make_shared<Match>(std::move(match));
    if (!s_workers.launch([pending, meta] {
            queueLobbyThread(std::move(*pending), meta);
        })) {
        net::tcp_close(pending->a.sock);
        net::tcp_close(pending->b.sock);
    }
}

void beginShutdown()
{
    s_stopping.store(true);
    s_workers.stopAccepting();
}

void waitForShutdown()
{
    s_workers.wait();
}

bool isShuttingDown()
{
    return s_stopping.load();
}

}  // namespace relay
