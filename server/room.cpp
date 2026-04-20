#include "room.h"

#include "relay.h"
#include "../net/framing.h"
#include "../net/socket.h"

#include <chrono>
#include <iostream>
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
    code_rng_state_ = t ? t : 0xC0FFEE0DDB0B0BAAULL;
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

void RoomRegistry::handleCreate(net::TcpSocket sock, uint32_t conn_id) {
    if (stopping.load()) { net::tcp_close(sock); return; }
    std::string code;
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
    }
    std::cerr << "[room] conn=" << conn_id << " created code=" << code << "\n";
    sendRoomInfo_(sock, code, kStatusWaiting, 1);
    roomLoop_(code, /*isHost=*/true);
}

void RoomRegistry::handleJoin(const std::string& code, net::TcpSocket sock, uint32_t conn_id) {
    if (stopping.load()) { net::tcp_close(sock); return; }
    bool entered = false;
    {
        std::unique_lock<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it == rooms.end()) {
            lk.unlock();
            sendRoomInfo_(sock, code, kStatusNotFound, 0);
            net::tcp_close(sock);
            std::cerr << "[room] conn=" << conn_id << " join " << code << " notfound\n";
            return;
        }
        auto& r = it->second;
        if (r.guestPresent || r.matchStarted) {
            const uint8_t peerCount =
                static_cast<uint8_t>((r.hostPresent ? 1 : 0) + (r.guestPresent ? 1 : 0));
            lk.unlock();
            sendRoomInfo_(sock, code, kStatusFull, peerCount);
            net::tcp_close(sock);
            std::cerr << "[room] conn=" << conn_id << " join " << code << " full\n";
            return;
        }
        r.guestSock     = sock;
        r.guestConn     = conn_id;
        r.guestPresent  = true;
        net::TcpSocket hs = r.hostSock;
        net::TcpSocket gs = r.guestSock;
        lk.unlock();
        sendRoomInfo_(hs, code, kStatusWaiting, 2);
        sendRoomInfo_(gs, code, kStatusWaiting, 2);
        entered = true;
    }
    if (entered) {
        std::cerr << "[room] conn=" << conn_id << " joined " << code << "\n";
        roomLoop_(code, /*isHost=*/false);
    }
}

void RoomRegistry::roomLoop_(const std::string& code, bool isHost) {
    // 내 소켓 사본 확보 (lock 밖에서 recv 하기 위함)
    net::TcpSocket mySock;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it == rooms.end()) return;
        auto& r = it->second;
        mySock = isHost ? r.hostSock : r.guestSock;
    }

    std::vector<uint8_t> stream;
    stream.reserve(256);
    bool leaveRequested   = false;
    bool peerStartedMatch = false;
    bool iAmStarter       = false;

    while (!stopping.load()) {
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
                        net::tcp_send_all(fwd, out.data(), out.size());
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
                        net::tcp_send_all(fwd, out.data(), out.size());
                    }
                }
                // 다른 타입(HELLO 등)은 이 단계에서는 무시
            }
        }

        if (leaveRequested) break;

        // 상태 변화 체크
        {
            std::lock_guard<std::mutex> lk(mu);
            auto it = rooms.find(code);
            if (it == rooms.end()) break;
            auto& r = it->second;

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

            m.a.sock     = r.hostSock;
            m.a.conn_id  = r.hostConn;
            m.b.sock     = r.guestSock;
            m.b.conn_id  = r.guestConn;
            m.seed       = nextSeed_();
            m.match_id   = nextMatchId_();
            rooms.erase(it);
        }
        std::cerr << "[room] code=" << code << " -> match id=" << m.match_id
                  << " seed=0x" << std::hex << m.seed << std::dec << "\n";
        relay::startPump(std::move(m));
        return;
    }

    if (peerStartedMatch) {
        // starter 가 내 소켓을 forwarderLoop 으로 이관함. 닫지 않고 리턴.
        return;
    }

    // 일반 종료(ROOM_LEAVE / EOF / shutdown) — 상대에게 알리고 내 소켓 닫음.
    // 주의: peer 통지(sendRoomInfo_)는 반드시 lock 보유 상태에서 보낸다.
    //   lock 밖에서 peerSock 사본으로 send 하는 동안, 상대 스레드가 자신의 종료
    //   경로에 들어가 같은 fd 를 tcp_close 하면 send 중에 fd 가 사라진다(UAC).
    //   ROOM_INFO 는 <50 B 단일 프레임이라 lock hold 시간 영향 미미.
    bool eraseRoom = false;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = rooms.find(code);
        if (it != rooms.end()) {
            auto& r = it->second;
            if (isHost) { r.hostPresent = false;  r.hostReady  = false; }
            else        { r.guestPresent = false; r.guestReady = false; }
            net::TcpSocket peerSock{};
            bool notifyPeer = false;
            if (isHost && r.guestPresent) { peerSock = r.guestSock; notifyPeer = true; }
            if (!isHost && r.hostPresent) { peerSock = r.hostSock;  notifyPeer = true; }
            if (notifyPeer) sendRoomInfo_(peerSock, code, kStatusGoneFull, 1);
            if (!r.hostPresent && !r.guestPresent) eraseRoom = true;
        }
    }

    if (eraseRoom) {
        std::lock_guard<std::mutex> lk(mu);
        rooms.erase(code);
    }

    net::tcp_close(mySock);
}

void RoomRegistry::shutdown() {
    if (stopping.exchange(true)) return;
    cv.notify_all();
    // roomLoop_ 들은 stopping 을 보고 자기 소켓을 닫으며 종료한다.
}

}  // namespace relay
