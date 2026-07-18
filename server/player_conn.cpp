#include "player_conn.h"

#include "matchmaker.h"
#include "room.h"
#include "relay.h"
#include "../net/framing.h"
#include "../meta/http_client.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

namespace {

// QUEUE_JOIN 또는 ROOM_CREATE 페이로드 끝의 [tok_len:1][token:N] 추출.
// 토큰 페이로드 앞에 다른 바이트가 있으면 offset 을 지정.  범위 초과 시 빈 문자열.
std::string extract_token(const std::vector<uint8_t>& pl, size_t offset)
{
    if (pl.size() < offset + 1) return {};
    const uint8_t n = pl[offset];
    if (n == 0) return {};
    if (pl.size() < offset + 1u + n) return {};
    return std::string(pl.begin() + offset + 1,
                       pl.begin() + offset + 1 + n);
}

// 첫 프레임(QUEUE_JOIN 등)과 같은 recv 에 실려 이미 파싱된 후속 프레임들과
// 아직 완성되지 않은 partial tail 을 원본 바이트 스트림으로 복원한다.
// build_frame 은 동일 payload 에 대해 bit-identical 하므로 재직렬화가 안전하다.
// 이 잔여분을 다음 단계(matchmaker 큐 / roomLoop_)의 수신 버퍼로 이관하지 않으면
// 그 프레임들(예: QUEUE_JOIN 직후의 QUEUE_CANCEL)이 조용히 유실된다.
std::vector<uint8_t> residual_stream(const std::vector<net::Frame>& frames,
                                     size_t next_idx,
                                     const std::vector<uint8_t>& tail)
{
    std::vector<uint8_t> out;
    for (size_t j = next_idx; j < frames.size(); ++j) {
        auto bytes = net::build_frame(frames[j].type, frames[j].payload);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    out.insert(out.end(), tail.begin(), tail.end());
    return out;
}

// meta 가 nullptr 또는 token 이 비어 있으면 unranked (player_id=0, elo=0).
// verify 실패면 std::nullopt → 호출자가 소켓 close.
struct AuthOutcome {
    int64_t     player_id = 0;
    int         elo = 0;
    std::string username;
    std::string token;
    std::string selected_icon_id{"default"};
};
std::optional<AuthOutcome>
authenticate(meta::client::MetaClient* meta, const std::string& token,
             uint32_t conn_id, const char* what)
{
    AuthOutcome o;
    if (!meta) {
        // unranked: meta 미연동 — 토큰이 있더라도 무시.
        std::cerr << "[conn " << conn_id << "] " << what
                  << " unranked (no meta)\n";
        return o;
    }
    if (token.empty()) {
        std::cerr << "[conn " << conn_id << "] " << what
                  << " missing token -> reject\n";
        return std::nullopt;
    }
    auto auth = meta->verify_token(token);
    if (!auth) {
        std::cerr << "[conn " << conn_id << "] " << what
                  << " meta verify failed -> reject\n";
        return std::nullopt;
    }
    o.player_id = auth->player_id;
    o.elo       = auth->elo;
    o.username  = auth->username;
    o.token     = token;
    o.selected_icon_id = auth->selected_icon_id.empty() ? "default" : auth->selected_icon_id;
    std::cerr << "[conn " << conn_id << "] " << what
              << " authed player_id=" << auth->player_id
              << " elo=" << auth->elo
              << " icon=" << o.selected_icon_id << "\n";
    return o;
}

} // namespace

// 첫 프레임(QUEUE_JOIN / ROOM_CREATE / ROOM_JOIN) 대기 제한 시간.
// 클라이언트는 TCP connect 직후 바로 첫 프레임을 보내므로 10초면 충분.
static constexpr auto kJoinTimeout  = std::chrono::seconds(10);
static constexpr auto kPollInterval = std::chrono::milliseconds(10);

void playerConnThread(net::TcpSocket sock, uint32_t conn_id,
                      Matchmaker& mm, RoomRegistry& rr,
                      meta::client::MetaClient* meta) {
    std::vector<uint8_t> stream;
    stream.reserve(64);

    const auto deadline = std::chrono::steady_clock::now() + kJoinTimeout;

    while (std::chrono::steady_clock::now() < deadline && !isShuttingDown()) {
        if (!net::tcp_recv_some(sock, stream)) {
            std::cerr << "[conn " << conn_id << "] disconnected before first frame\n";
            net::tcp_close(sock);
            return;
        }

        if (!stream.empty()) {
            std::vector<net::Frame> frames;
            net::parse_frames(stream, frames);
            for (size_t i = 0; i < frames.size(); ++i) {
                const net::Frame& f = frames[i];
                if (f.type == net::MsgType::QUEUE_JOIN) {
                    // 페이로드: [tok_len:1][token:N]
                    std::string tok = extract_token(f.payload, 0);
                    auto auth = authenticate(meta, tok, conn_id, "QUEUE_JOIN");
                    if (!auth) { net::tcp_close(sock); return; }

                    PlayerInfo pi;
                    pi.sock      = std::move(sock);
                    pi.conn_id   = conn_id;
                    pi.player_id = auth->player_id;
                    pi.elo       = auth->elo;
                    pi.username  = std::move(auth->username);
                    pi.token     = std::move(auth->token);
                    pi.selected_icon_id = std::move(auth->selected_icon_id);
                    // 같은 recv 로 이미 도착한 후속 프레임/부분 바이트를 큐
                    // 폴링 버퍼로 이관 (즉시 QUEUE_CANCEL 유실 방지).
                    pi.streamBuf = residual_stream(frames, i + 1, stream);
                    std::cerr << "[conn " << conn_id << "] QUEUE_JOIN -> queued\n";
                    mm.enqueue(std::move(pi));
                    return;
                }
                if (f.type == net::MsgType::QUEUE_CANCEL) {
                    std::cerr << "[conn " << conn_id << "] QUEUE_CANCEL before queued\n";
                    net::tcp_close(sock);
                    return;
                }
                if (f.type == net::MsgType::ROOM_CREATE) {
                    // 페이로드: [tok_len:1][token:N]
                    std::string tok = extract_token(f.payload, 0);
                    auto auth = authenticate(meta, tok, conn_id, "ROOM_CREATE");
                    if (!auth) { net::tcp_close(sock); return; }
                    std::cerr << "[conn " << conn_id << "] ROOM_CREATE\n";
                    rr.handleCreate(std::move(sock), conn_id,
                                    auth->player_id, auth->elo,
                                    auth->username, auth->token,
                                    auth->selected_icon_id,
                                    residual_stream(frames, i + 1, stream));
                    return;
                }
                if (f.type == net::MsgType::ROOM_JOIN) {
                    if (f.payload.size() < 1) continue;
                    const uint8_t n = f.payload[0];
                    constexpr uint8_t kMaxCodeLen = 5;
                    if (n == 0 || n > kMaxCodeLen ||
                        f.payload.size() < 1u + n) continue;
                    std::string code(f.payload.begin() + 1,
                                     f.payload.begin() + 1 + n);
                    // 코드 뒤에 [tok_len:1][token:N]
                    std::string tok = extract_token(f.payload, 1u + n);
                    auto auth = authenticate(meta, tok, conn_id, "ROOM_JOIN");
                    if (!auth) { net::tcp_close(sock); return; }
                    std::cerr << "[conn " << conn_id << "] ROOM_JOIN " << code << "\n";
                    rr.handleJoin(code, std::move(sock), conn_id,
                                  auth->player_id, auth->elo,
                                  auth->username, auth->token,
                                  auth->selected_icon_id,
                                  residual_stream(frames, i + 1, stream));
                    return;
                }
                // HELLO 등 낯선 프레임은 초기 phase 에서는 무시 + 계속 대기
            }
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    if (!isShuttingDown()) {
        std::cerr << "[conn " << conn_id << "] first-frame timeout -> close\n";
    }
    net::tcp_close(sock);
}

}  // namespace relay
