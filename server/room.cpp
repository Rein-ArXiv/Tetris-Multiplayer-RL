#include "room.h"

#include "relay.h"
#include "log.h"
#include "match_uuid.h"
#include "../net/framing.h"
#include "../net/socket.h"

#include <chrono>
#include <functional>
#include <random>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

namespace {

// base32 알파벳 — 혼동 쉬운 0/O/1/I 제외 (plan §D.1)
constexpr char   kCodeAlphabet[]    = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr size_t kCodeAlphabetN     = sizeof(kCodeAlphabet) - 1;
constexpr size_t kCodeLen           = 5;
constexpr auto   kPollInterval      = std::chrono::milliseconds(10);

// roomLoop_ 대기 단계 데드라인 — queueLobbyThread 의 kConfirmTimeout 과 같은 결.
// 방이 무기한 열려 있으면 워커 슬롯·IP admission·세션 lease 가 그만큼 잠긴 채
// 새 연결을 굶기므로, 진행이 없는 방은 서버가 먼저 정리한다.
constexpr auto   kRoomGuestWaitTimeout = std::chrono::minutes(15);  // 개설 후 게스트 무입장
constexpr auto   kRoomReadyTimeout     = std::chrono::seconds(60);  // 게스트 입장 후 READY 미확정

// ROOM_INFO status 바이트 (plan §D.2 / framing.h)
constexpr uint8_t kStatusWaiting    = 0;
constexpr uint8_t kStatusFull       = 1;
constexpr uint8_t kStatusNotFound   = 2;
constexpr uint8_t kStatusGoneFull   = 3;

uint64_t xorshift64_(uint64_t& s) {
    uint64_t x = s;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    s = x;
    return x;
}

}  // namespace

RoomRegistry::RoomRegistry() {
    using clock = std::chrono::high_resolution_clock;
    const auto t = static_cast<uint64_t>(clock::now().time_since_epoch().count());
    // 룸코드는 추측되면 남의 방에 난입할 수 있으므로 부팅 시각만으로 시드하지
    // 않는다 — random_device(주요 플랫폼에서 OS CSPRNG)를 섞어 예측을 차단.
    std::random_device rd;
    const uint64_t r = (static_cast<uint64_t>(rd()) << 32) | rd();
    code_rng_state_ = (t ^ r) ? (t ^ r) : 0xC0FFEE0DDB0B0BAAULL;
    // seed stream 은 다른 상태 — 한 프로세스 안에서 matchmaker 와 충돌 최소화.
    seed_state_     = (t ? t : 0xDEADBEEFCAFEBABEULL) ^ 0x9E3779B97F4A7C15ULL;
}

std::string RoomRegistry::generateCode_() {
    // mu 잡힘. 충돌 나면 재시도 — 실질적으로 매우 드물다 (32^5 = 33M 조합).
    for (int attempt = 0; attempt < 32; ++attempt) {
        std::string c(kCodeLen, 'A');
        uint64_t x = xorshift64_(code_rng_state_);
        for (size_t i = 0; i < kCodeLen; ++i) {
            c[i] = kCodeAlphabet[x % kCodeAlphabetN];
            x /= kCodeAlphabetN;
            if (x == 0) x = xorshift64_(code_rng_state_);
        }
        if (rooms.find(c) == rooms.end()) return c;
    }
    return {};  // 상상 속 병리적 충돌
}

uint64_t RoomRegistry::nextSeed_()    { return xorshift64_(seed_state_); }
uint32_t RoomRegistry::nextMatchId_() { return next_match_id_++; }

void RoomRegistry::sendRoomInfo_(const net::TcpSocket& sock, const std::string& code,
                                  uint8_t status, uint8_t peerCount) {
    // ROOM_INFO payload: [code_len:1][code:N][status:1][peer_count:1]
    std::vector<uint8_t> payload;
    payload.reserve(1 + code.size() + 2);
    payload.push_back(static_cast<uint8_t>(code.size()));
    for (char c : code) payload.push_back(static_cast<uint8_t>(c));
    payload.push_back(status);
    payload.push_back(peerCount);
    auto f = net::build_frame(net::MsgType::ROOM_INFO, payload);
    net::tcp_send_all(sock, f.data(), f.size());
}

void RoomRegistry::sendRoomInfoIfCurrent_(
    const net::TcpSocket& sock, const std::string& code,
    uint8_t status, uint8_t peerCount, uint64_t expectedVersion) {
    // 새 상태가 먼저 기록됐다면 이전 알림을 생략한다. 이전 알림이 이미 송신
    // 중이면 새 알림은 같은 방의 게이트 뒤에서 기다리므로 wire 순서도 보장된다.
    const size_t shard = std::hash<std::string>{}(code) % kRoomSendShardCount;
    std::lock_guard<std::mutex> sendLk(roomSendMu_[shard]);
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it == rooms.end() ||
            it->second.roomInfoVersion != expectedVersion) {
            return;
        }
    }
    sendRoomInfo_(sock, code, status, peerCount);
}

bool RoomRegistry::sendRoomFrame_(const std::string& code,
                                  const net::TcpSocket& sock,
                                  const std::vector<uint8_t>& frame) {
    const size_t shard = std::hash<std::string>{}(code) % kRoomSendShardCount;
    std::lock_guard<std::mutex> sendLk(roomSendMu_[shard]);
    return net::tcp_send_all(sock, frame.data(), frame.size());
}

void RoomRegistry::handleCreate(net::TcpSocket sock, uint32_t conn_id,
                                int64_t player_id, int elo,
                                const std::string& username, const std::string& token,
                                const std::string& selected_icon_id,
                                std::shared_ptr<PlayerSessionLease> session_lease,
                                std::shared_ptr<IpAdmission> ip_session,
                                std::vector<uint8_t> streamPrefix) {
    if (stopping.load()) { net::tcp_close(sock); return; }
    std::string code;
    uint64_t roomInfoVersion = 0;
    {
        std::unique_lock<std::mutex> lk(mu);
        code = generateCode_();
        if (code.empty()) {
            lk.unlock();
            net::tcp_close(sock);
            return;
        }
        Entry& r       = rooms[code];
        r.code         = code;
        r.hostSock     = sock;
        r.hostConn     = conn_id;
        r.hostPresent  = true;
        r.hostPlayerId = player_id;
        r.hostElo      = elo;
        r.hostUsername = username;
        r.hostToken    = token;
        r.hostSelectedIconId = selected_icon_id.empty() ? "default" : selected_icon_id;
        r.hostSessionLease = std::move(session_lease);
        r.hostIpSession    = std::move(ip_session);
        roomInfoVersion = r.roomInfoVersion = next_room_info_version_++;
    }
    RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
              << " created code=" << code);
    sendRoomInfoIfCurrent_(sock, code, kStatusWaiting, 1, roomInfoVersion);
    roomLoop_(code, /*isHost=*/true, std::move(streamPrefix));
}

void RoomRegistry::handleJoin(const std::string& code, net::TcpSocket sock, uint32_t conn_id,
                              int64_t player_id, int elo,
                              const std::string& username, const std::string& token,
                              const std::string& selected_icon_id,
                              std::shared_ptr<PlayerSessionLease> session_lease,
                              std::shared_ptr<IpAdmission> ip_session,
                              std::vector<uint8_t> streamPrefix) {
    if (stopping.load()) { net::tcp_close(sock); return; }
    bool entered = false;
    uint64_t roomInfoVersion = 0;
    {
        // send gate를 먼저 잡은 뒤 guestPresent를 공개한다. 반대 순서면 host
        // roomLoop가 그 사이 guest를 발견하고 CHAT/READY를 ROOM_INFO보다 먼저
        // 보낼 수 있다. 모든 중첩 잠금은 send gate -> state mu 순서를 따른다.
        const size_t shard = std::hash<std::string>{}(code) % kRoomSendShardCount;
        std::unique_lock<std::mutex> sendLk(roomSendMu_[shard]);
        std::unique_lock<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it == rooms.end()) {
            lk.unlock();
            sendRoomInfo_(sock, code, kStatusNotFound, 0);
            net::tcp_close(sock);
            RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
                      << " close: join " << code << " notfound match_uuid=-");
            return;
        }
        auto& r = it->second;
        if (r.guestPresent || r.matchStarted) {
            const uint8_t peerCount =
                static_cast<uint8_t>((r.hostPresent ? 1 : 0) + (r.guestPresent ? 1 : 0));
            lk.unlock();
            sendRoomInfo_(sock, code, kStatusFull, peerCount);
            net::tcp_close(sock);
            RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
                      << " close: join " << code << " full match_uuid=-");
            return;
        }
        r.guestSock     = sock;
        r.guestConn     = conn_id;
        r.guestPresent  = true;
        r.guestPlayerId = player_id;
        r.guestElo      = elo;
        r.guestUsername = username;
        r.guestToken    = token;
        r.guestSelectedIconId = selected_icon_id.empty() ? "default" : selected_icon_id;
        r.guestSessionLease = std::move(session_lease);
        r.guestIpSession    = std::move(ip_session);
        net::TcpSocket hs = r.hostSock;
        net::TcpSocket gs = r.guestSock;
        roomInfoVersion = r.roomInfoVersion = next_room_info_version_++;
        lk.unlock();
        {
            std::lock_guard<std::mutex> stateLk(mu);
            auto current = rooms.find(code);
            entered = current != rooms.end() &&
                      current->second.roomInfoVersion == roomInfoVersion;
        }
        if (entered) {
            // 두 참가자의 ROOM_INFO 사이에도 READY/CHAT이 끼지 않는다.
            sendRoomInfo_(hs, code, kStatusWaiting, 2);
            sendRoomInfo_(gs, code, kStatusWaiting, 2);
        }
    }
    if (entered) {
        RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
                  << " joined " << code);
        roomLoop_(code, /*isHost=*/false, std::move(streamPrefix));
    }
}

void RoomRegistry::roomLoop_(const std::string& code, bool isHost,
                             std::vector<uint8_t> streamPrefix) {
    // 내 소켓 사본 확보 (lock 밖에서 recv 하기 위함)
    net::TcpSocket mySock;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it == rooms.end()) return;
        auto& r = it->second;
        mySock = isHost ? r.hostSock : r.guestSock;
    }

    // playerConnThread 가 첫 프레임과 함께 끌어온 잔여 바이트를 수신 버퍼의
    // 초기값으로 사용 — ROOM_CREATE/JOIN 직후 같은 recv 에 실려온 READY/CHAT
    // 등이 유실되지 않는다.
    std::vector<uint8_t> stream = std::move(streamPrefix);
    stream.reserve(256);
    bool leaveRequested   = false;
    bool peerStartedMatch = false;
    bool iAmStarter       = false;
    bool timedOut         = false;

    // 단계별 데드라인. 게스트 스레드의 대기는 시작부터 끝까지 READY 단계이고
    // (호스트가 떠난 방은 재입장 경로가 없어 더 진행될 수 없다), 호스트 스레드는
    // 게스트 입·퇴장을 관측하는 순간 단계가 전환되므로 아래 상태 체크에서
    // bothPresent 변화를 보고 데드라인을 다시 건다.
    auto armDeadline = [](bool bothPresent) {
        const auto now = std::chrono::steady_clock::now();
        return bothPresent
            ? std::chrono::steady_clock::time_point(now + kRoomReadyTimeout)
            : std::chrono::steady_clock::time_point(now + kRoomGuestWaitTimeout);
    };
    bool bothPresentPrev = !isHost;  // 게스트는 입장 시점에 이미 양측 재실
    auto deadline        = armDeadline(bothPresentPrev);

    while (!stopping.load()) {
        // 데드라인은 활동(채팅/READY 토글)으로 연장하지 않는다 — 활동 기준이면
        // 채팅만 계속 보내며 워커 슬롯을 무한정 점유할 수 있다.
        if (std::chrono::steady_clock::now() >= deadline) {
            RLOG_INFO("[room] code=" << code << " "
                      << (isHost ? "host" : "guest")
                      << (bothPresentPrev ? " ready-wait" : " guest-wait")
                      << " close: timeout match_uuid=-");
            timedOut = true;
            break;
        }

        if (!net::tcp_recv_some(mySock, stream)) {
            // EOF — 소켓 닫힘
            break;
        }

        if (!stream.empty()) {
            std::vector<net::Frame> frames;
            net::parse_frames(stream, frames);
            for (const auto& f : frames) {
                if (f.type == net::MsgType::READY) {
                    const bool ready = !f.payload.empty() && f.payload[0] != 0;
                    net::TcpSocket fwd{};
                    bool hasFwd = false;
                    {
                        std::lock_guard<std::mutex> lk(mu);
                        auto it = rooms.find(code);
                        if (it != rooms.end()) {
                            auto& r = it->second;
                            if (isHost) r.hostReady  = ready;
                            else        r.guestReady = ready;
                            if (isHost && r.guestPresent) { fwd = r.guestSock; hasFwd = true; }
                            if (!isHost && r.hostPresent) { fwd = r.hostSock;  hasFwd = true; }
                        }
                    }
                    if (hasFwd) {
                        std::vector<uint8_t> p; p.push_back(ready ? 1 : 0);
                        auto out = net::build_frame(net::MsgType::READY, p);
                        sendRoomFrame_(code, fwd, out);
                    }
                } else if (f.type == net::MsgType::ROOM_LEAVE) {
                    leaveRequested = true;
                } else if (f.type == net::MsgType::CHAT) {
                    // 대기 중 채팅 — 상대에게 그대로 전달
                    net::TcpSocket fwd{};
                    bool hasFwd = false;
                    {
                        std::lock_guard<std::mutex> lk(mu);
                        auto it = rooms.find(code);
                        if (it != rooms.end()) {
                            auto& r = it->second;
                            if (isHost && r.guestPresent) { fwd = r.guestSock; hasFwd = true; }
                            if (!isHost && r.hostPresent) { fwd = r.hostSock;  hasFwd = true; }
                        }
                    }
                    if (hasFwd) {
                        auto out = net::build_frame(net::MsgType::CHAT, f.payload);
                        sendRoomFrame_(code, fwd, out);
                    }
                }
                // 다른 타입(HELLO 등)은 이 단계에서는 무시
            }
        }

        if (leaveRequested) break;

        // 상태 변화 체크
        bool bothPresentNow = false;
        {
            std::lock_guard<std::mutex> lk(mu);
            auto it = rooms.find(code);
            if (it == rooms.end()) break;
            auto& r = it->second;
            bothPresentNow = r.hostPresent && r.guestPresent;

            if (r.matchStarted) {
                // 상대가 starter 로 선점함 — 내 read 루프를 내려놓고 exit 플래그 세팅
                peerStartedMatch = true;
                if (isHost) r.hostExited = true;
                else        r.guestExited = true;
                cv.notify_all();
                break;
            }

            if (r.hostPresent && r.guestPresent && r.hostReady && r.guestReady) {
                r.matchStarted = true;
                iAmStarter     = true;
                cv.notify_all();
                break;
            }
        }

        // 호스트의 대기 단계 전환: 게스트 입장 → READY 대기(60s), 게스트 퇴장 →
        // 다시 게스트 대기(15m). 게스트 스레드는 단계가 바뀌지 않으므로 최초
        // 데드라인을 유지한다 — 호스트가 떠난 zombie 방에 눌러앉는 것도 이
        // 데드라인이 정리한다.
        if (isHost && bothPresentNow != bothPresentPrev) {
            bothPresentPrev = bothPresentNow;
            deadline = armDeadline(bothPresentNow);
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    if (iAmStarter) {
        // 상대가 read 루프를 내려놓을 때까지 대기 — 이후 둘 다 소켓을 forwarderLoop
        // 에 넘긴다. 같은 fd 를 두 스레드가 동시에 recv 하지 않도록 보장.
        Match m{};
        {
            std::unique_lock<std::mutex> lk(mu);
            cv.wait(lk, [&] {
                if (stopping.load()) return true;
                auto it = rooms.find(code);
                if (it == rooms.end()) return true;
                auto& r = it->second;
                if (isHost)  return r.guestExited || !r.guestPresent;
                else         return r.hostExited  || !r.hostPresent;
            });

            auto it = rooms.find(code);
            if (it == rooms.end() || stopping.load()) {
                // 상대 사라짐 — 내 소켓만 닫고 종료
                if (it != rooms.end()) rooms.erase(it);
                net::tcp_close(mySock);
                return;
            }
            auto& r = it->second;
            if (!(r.hostPresent && r.guestPresent)) {
                // 상대가 매치 시작 직전에 퇴장 — 혼자 남은 소켓 정리
                net::tcp_close(mySock);
                rooms.erase(it);
                return;
            }

            m.a.sock      = r.hostSock;
            m.a.conn_id   = r.hostConn;
            m.a.player_id = r.hostPlayerId;
            m.a.elo       = r.hostElo;
            m.a.username  = r.hostUsername;
            m.a.token     = r.hostToken;
            m.a.selected_icon_id = r.hostSelectedIconId;
            m.a.session_lease = r.hostSessionLease;
            m.a.ip_session    = r.hostIpSession;
            m.b.sock      = r.guestSock;
            m.b.conn_id   = r.guestConn;
            m.b.player_id = r.guestPlayerId;
            m.b.elo       = r.guestElo;
            m.b.username  = r.guestUsername;
            m.b.token     = r.guestToken;
            m.b.selected_icon_id = r.guestSelectedIconId;
            m.b.session_lease = r.guestSessionLease;
            m.b.ip_session    = r.guestIpSession;
            m.seed        = nextSeed_();
            m.match_id    = nextMatchId_();
            m.match_uuid  = new_match_uuid();
            rooms.erase(it);
        }
        RLOG_INFO("[room] code=" << code << " -> match id=" << m.match_id
                  << " uuid=" << m.match_uuid
                  << " player_id=" << m.a.player_id << " x " << m.b.player_id
                  << " seed=" << log_hex(m.seed));
        relay::startPump(std::move(m), meta_);
        return;
    }

    if (peerStartedMatch) {
        // starter 가 내 소켓을 forwarderLoop 으로 이관함. 닫지 않고 리턴.
        return;
    }

    if (timedOut) {
        // 정중한 종료 통지: 데드라인 초과로 닫을 때 EOF 만 던지면 클라이언트는
        // 네트워크 오류로 오인한다. 전용 타임아웃 status 가 없어 gonefull(방 종료)
        // 을 재사용해 대기 화면을 정리할 기회를 준다. 상대 스레드가 같은 소켓에
        // READY/CHAT 을 포워딩 중일 수 있으므로 방 게이트로 직렬화.
        const size_t shard = std::hash<std::string>{}(code) % kRoomSendShardCount;
        std::lock_guard<std::mutex> sendLk(roomSendMu_[shard]);
        sendRoomInfo_(mySock, code, kStatusGoneFull, 1);
    }

    // 일반 종료(ROOM_LEAVE / EOF / 대기 타임아웃 / shutdown) — 상대에게 알리고
    // 내 소켓 닫음. peer 통지는 상태 mutex 밖에서 보내되 방별 게이트로 직렬화한다.
    // tcp_send_all 이 블록해도 다른 방의 처리는 계속되며, 버전 검증으로
    // 새 입장 뒤 오래된 gonefull 이 도착하는 상태 역전을 막는다.
    net::TcpSocket peerSock{};
    bool notifyPeer = false;
    uint64_t roomInfoVersion = 0;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it != rooms.end()) {
            auto& r = it->second;
            // 떠나는 쪽의 세션 lease 와 per-IP 세션 슬롯은 즉시 반납한다 —
            // 상대가 남아 방 Entry 가 유지되는 동안에도 이 플레이어가 새 연결로
            // 재인증할 수 있어야 하고, 타임아웃 정리 시 자원 회수가 방 소멸
            // 시점까지 미뤄지지 않게 한다. (이 소켓은 아래에서 닫힌다.)
            if (isHost) {
                r.hostPresent = false;  r.hostReady  = false;
                r.hostSessionLease.reset(); r.hostIpSession.reset();
            } else {
                r.guestPresent = false; r.guestReady = false;
                r.guestSessionLease.reset(); r.guestIpSession.reset();
            }
            if (isHost && r.guestPresent) { peerSock = r.guestSock; notifyPeer = true; }
            if (!isHost && r.hostPresent) { peerSock = r.hostSock;  notifyPeer = true; }
            roomInfoVersion = r.roomInfoVersion = next_room_info_version_++;
            if (!r.hostPresent && !r.guestPresent) rooms.erase(it);
        }
    }

    if (notifyPeer) {
        sendRoomInfoIfCurrent_(peerSock, code, kStatusGoneFull, 1,
                               roomInfoVersion);
    }

    net::tcp_close(mySock);
}

void RoomRegistry::shutdown() {
    if (stopping.exchange(true)) return;
    cv.notify_all();
    // roomLoop_ 들은 stopping 을 보고 자기 소켓을 닫으며 종료한다.
}

}  // namespace relay
