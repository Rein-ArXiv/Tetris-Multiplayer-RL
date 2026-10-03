#include "room.h"
#include "room_code.h"

#include "relay.h"
#include "log.h"
#include "match_uuid.h"
#include "../net/framing.h"
#include "../net/socket.h"

#include <chrono>
#include <functional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace relay {

namespace {

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

}  // namespace

RoomRegistry::RoomRegistry() = default;

std::string RoomRegistry::generateCode_() {
    // mu remains held from candidate search through insertion in handleCreate.
    auto code = selectRoomCode(roomCodeRandomWord, [this](const std::string& c) {
        return rooms.find(c) != rooms.end();
    });
    return code ? std::move(*code) : std::string{};
}

uint64_t RoomRegistry::nextSeed_()    { return seed_src_.next(); }
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
    try {
        uint64_t roomInfoVersion = 0;
        {
            std::unique_lock<std::mutex> lk(mu);
            // Serialize admission with shutdown; the early check is only a fast path.
            if (stopping.load()) {
                lk.unlock();
                net::tcp_close(sock);
                return;
            }
            code = generateCode_();
            if (code.empty()) {
                lk.unlock();
                net::tcp_close(sock);
                return;
            }
            // Prepare potentially allocating fields before publishing the entry.
            // A failed string copy must not leave a room with no owning roomLoop.
            static_assert(std::is_nothrow_move_constructible_v<Entry>);
            Entry r;
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
            roomInfoVersion = r.roomInfoVersion = next_room_info_version_;
            rooms.emplace(code, std::move(r));
            ++next_room_info_version_;
        }
        RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
                  << " created code=" << code);
        sendRoomInfoIfCurrent_(sock, code, kStatusWaiting, 1, roomInfoVersion);
        roomLoop_(code, /*isHost=*/true, sock, std::move(streamPrefix));
    } catch (...) {
        abortRoom_(code, sock);
        throw;
    }
}

void RoomRegistry::handleJoin(const std::string& code, net::TcpSocket sock, uint32_t conn_id,
                              int64_t player_id, int elo,
                              const std::string& username, const std::string& token,
                              const std::string& selected_icon_id,
                              std::shared_ptr<PlayerSessionLease> session_lease,
                              std::shared_ptr<IpAdmission> ip_session,
                              std::vector<uint8_t> streamPrefix) {
    if (stopping.load()) { net::tcp_close(sock); return; }
    try {
        bool entered = false;
        uint64_t roomInfoVersion = 0;
        {
            // send gate를 먼저 잡은 뒤 guestPresent를 공개한다. 반대 순서면 host
            // roomLoop가 그 사이 guest를 발견하고 CHAT/READY를 ROOM_INFO보다 먼저
            // 보낼 수 있다. 모든 중첩 잠금은 send gate -> state mu 순서를 따른다.
            const size_t shard = std::hash<std::string>{}(code) % kRoomSendShardCount;
            std::unique_lock<std::mutex> sendLk(roomSendMu_[shard]);
            std::unique_lock<std::mutex> lk(mu);
            if (stopping.load()) {
                lk.unlock();
                net::tcp_close(sock);
                return;
            }
            auto it = rooms.find(code);
            if (it == rooms.end()) {
                lk.unlock();
                sendRoomInfo_(sock, code, kStatusNotFound, 0);
                net::tcp_close(sock);
                RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
                          << " close: join " << code << " notfound match_uuid=-");
                return;
            }
            const auto& current = it->second;
            if (current.guestPresent || current.matchStarted) {
                const uint8_t peerCount =
                    static_cast<uint8_t>((current.hostPresent ? 1 : 0) + (current.guestPresent ? 1 : 0));
                lk.unlock();
                sendRoomInfo_(sock, code, kStatusFull, peerCount);
                net::tcp_close(sock);
                RLOG_INFO("[room] conn=" << conn_id << " player_id=" << player_id
                          << " close: join " << code << " full match_uuid=-");
                return;
            }
            Entry r = current; // Copy before changing the live room.
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
            roomInfoVersion = r.roomInfoVersion = next_room_info_version_;
            static_assert(std::is_nothrow_move_assignable_v<Entry>);
            it->second = std::move(r);
            ++next_room_info_version_;
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
            roomLoop_(code, /*isHost=*/false, sock, std::move(streamPrefix));
        } else {
            // The published guest still owns a slot if a concurrent state change
            // invalidated its initial notice. No reader loop will reclaim it.
            abortRoom_(code, sock);
            net::tcp_close(sock);
        }
    } catch (...) {
        abortRoom_(code, sock);
        throw;
    }
}

// An abandoned owner path must not leave an entry without its reader loop.
// Compare owning handle identities, not reusable fd numbers or only the code.
void RoomRegistry::abortRoom_(const std::string& code, const net::TcpSocket& owner) {
    {
        std::lock_guard<std::mutex> lock(mu);
        const auto it = rooms.find(code);
        if (it == rooms.end()) return;
        const auto same = [&](const net::TcpSocket& socket) {
            return (owner.fdh || owner.transport) &&
                   socket.fdh == owner.fdh && socket.transport == owner.transport;
        };
        if (!same(it->second.hostSock) && !same(it->second.guestSock)) return;
        net::tcp_close(it->second.hostSock);
        net::tcp_close(it->second.guestSock);
        rooms.erase(it);
    }
    cv.notify_all();
}

bool RoomRegistry::ownsSlot_(const Entry& entry, bool isHost,
                             const net::TcpSocket& expected) {
    const auto& socket = isHost ? entry.hostSock : entry.guestSock;
    const bool present = isHost ? entry.hostPresent : entry.guestPresent;
    return present && (expected.fdh || expected.transport) &&
           socket.fdh == expected.fdh && socket.transport == expected.transport;
}

void RoomRegistry::roomLoop_(const std::string& code, bool isHost,
                             const net::TcpSocket& expected,
                             std::vector<uint8_t> streamPrefix) {
    // Keep the caller's owning identity: the same code/role may be reused.
    net::TcpSocket mySock = expected;
    bool ownsSlot = false;
    {
        std::lock_guard<std::mutex> lk(mu);
        const auto it = rooms.find(code);
        ownsSlot = it != rooms.end() && ownsSlot_(it->second, isHost, mySock);
    }
    if (!ownsSlot) {
        net::tcp_close(mySock);
        return;
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
            if (!net::parse_frames(stream, frames)) {
                RLOG_WARN("[room] code=" << code << " close: invalid frame boundary");
                break; // Use the common room/peer/socket cleanup below.
            }
            for (const auto& f : frames) {
                if (f.type == net::MsgType::READY) {
                    if (f.payload.size() != 1 || f.payload[0] > 1) {
                        leaveRequested = true;
                        break; // Common cleanup releases the room slot and socket.
                    }
                    const bool ready = f.payload[0] == 1;
                    net::TcpSocket fwd{};
                    bool hasFwd = false;
                    {
                        std::lock_guard<std::mutex> lk(mu);
                        auto it = rooms.find(code);
                        if (it != rooms.end() && ownsSlot_(it->second, isHost, mySock)) {
                            auto& r = it->second;
                            if (isHost) r.hostReady  = ready;
                            else        r.guestReady = ready;
                            if (isHost && r.guestPresent) { fwd = r.guestSock; hasFwd = true; }
                            if (!isHost && r.hostPresent) { fwd = r.hostSock;  hasFwd = true; }
                        } else {
                            leaveRequested = true;
                        }
                    }
                    if (hasFwd) {
                        std::vector<uint8_t> p; p.push_back(ready ? 1 : 0);
                        auto out = net::build_frame(net::MsgType::READY, p);
                        sendRoomFrame_(code, fwd, out);
                    }
                } else if (f.type == net::MsgType::ROOM_LEAVE) {
                    leaveRequested = true;
                    break;
                } else if (f.type == net::MsgType::CHAT) {
                    // 대기 중 채팅 — 상대에게 그대로 전달
                    net::TcpSocket fwd{};
                    bool hasFwd = false;
                    {
                        std::lock_guard<std::mutex> lk(mu);
                        auto it = rooms.find(code);
                        if (it != rooms.end() && ownsSlot_(it->second, isHost, mySock)) {
                            auto& r = it->second;
                            if (isHost && r.guestPresent) { fwd = r.guestSock; hasFwd = true; }
                            if (!isHost && r.hostPresent) { fwd = r.hostSock;  hasFwd = true; }
                        } else {
                            leaveRequested = true;
                        }
                    }
                    if (hasFwd) {
                        auto out = net::build_frame(net::MsgType::CHAT, f.payload);
                        sendRoomFrame_(code, fwd, out);
                    }
                }
                if (leaveRequested) break;
                // 다른 타입(HELLO 등)은 이 단계에서는 무시
            }
        }

        if (leaveRequested) break;

        // 상태 변화 체크
        bool bothPresentNow = false;
        {
            std::lock_guard<std::mutex> lk(mu);
            auto it = rooms.find(code);
            if (it == rooms.end() || !ownsSlot_(it->second, isHost, mySock)) break;
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
                if (it == rooms.end() || !ownsSlot_(it->second, isHost, mySock)) return true;
                auto& r = it->second;
                if (isHost)  return r.guestExited || !r.guestPresent;
                else         return r.hostExited  || !r.hostPresent;
            });

            auto it = rooms.find(code);
            const bool stillOwns = it != rooms.end() && ownsSlot_(it->second, isHost, mySock);
            if (!stillOwns || stopping.load()) {
                // Never erase a replacement room reached by an old reader.
                if (stillOwns) rooms.erase(it);
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
    std::shared_ptr<PlayerSessionLease> retiredLease;
    std::shared_ptr<IpAdmission> retiredIp;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it != rooms.end() && ownsSlot_(it->second, isHost, mySock)) {
            auto& r = it->second;
            // Move departure leases out; release them after mu, before peer I/O.
            // A surviving room must not keep the departed admission alive.
            // Other aliases, if any, may still defer final release.
            if (isHost) {
                r.hostPresent = false;  r.hostReady  = false;
                r.hostSock = {}; // mySock retains the departing owner until close below.
                retiredLease = std::move(r.hostSessionLease);
                retiredIp = std::move(r.hostIpSession);
            } else {
                r.guestPresent = false; r.guestReady = false;
                r.guestSock = {};
                retiredLease = std::move(r.guestSessionLease);
                retiredIp = std::move(r.guestIpSession);
            }
            if (isHost && r.guestPresent) { peerSock = r.guestSock; notifyPeer = true; }
            if (!isHost && r.hostPresent) { peerSock = r.hostSock;  notifyPeer = true; }
            roomInfoVersion = r.roomInfoVersion = next_room_info_version_++;
            if (!r.hostPresent && !r.guestPresent) rooms.erase(it);
            // A starter may be waiting for this presence change, not reader-exit.
            cv.notify_all();
        }
    }
    retiredLease.reset();
    retiredIp.reset();

    if (notifyPeer) {
        sendRoomInfoIfCurrent_(peerSock, code, kStatusGoneFull, 1,
                               roomInfoVersion);
    }

    net::tcp_close(mySock);
}

void RoomRegistry::shutdown() {
    {
        std::lock_guard<std::mutex> lk(mu);
        if (stopping.exchange(true)) return;
    }
    cv.notify_all();
    // roomLoop_ 들은 stopping 을 보고 자기 소켓을 닫으며 종료한다.
}

}  // namespace relay
