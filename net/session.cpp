#include "session.h"
#include <random>
#include <chrono>
#include <iostream>
#include <thread>

namespace net {

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

Session::Session() {}
Session::~Session() { Close(); }

LinkStatus Session::linkStatus() const {
    if (connectionFailed.load()) return LinkStatus::Lost;
    if (!ready.load()) return LinkStatus::OK;
    int64_t last = lastPongMs.load();
    if (last == 0) return LinkStatus::OK;  // 첫 PONG 전에는 판단 유예
    int64_t ago = now_ms() - last;
    if (ago >= 10000) return LinkStatus::Lost;
    if (ago >=  2000) return LinkStatus::Stalled;
    return LinkStatus::OK;
}

bool Session::Host(uint16_t port, const SeedParams& sp) {
    if (listening) return false;

    // Close() 이후 재사용을 위한 상태 리셋. 같은 Session 객체를 재활용할 때
    // 이전 세션의 sendQ / HASH 상태가 남아 새 연결의 ioThread 로 유출되는 것을
    // 방지한다 (stale backlog 의 재연결 변종).
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    {
        std::lock_guard<std::mutex> lk(inMu);
        remoteInputs.clear();
    }
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }

    { std::lock_guard<std::mutex> lk(seedMu); seedParams = sp; }
    listening = true;
    ath = std::thread(&Session::acceptThread, this, port);
    return true;
}

bool Session::Adopt(TcpSocket socket, Role role, uint64_t seed,
                    uint32_t start_tick, uint8_t input_delay) {
    if (!socket.valid()) return false;

    // Close() 이후 재사용을 위한 상태 리셋 (sendQ / HASH 포함)
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    listening = false;
    {
        std::lock_guard<std::mutex> lk(inMu);
        remoteInputs.clear();
    }
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }

    {
        std::lock_guard<std::mutex> lk(seedMu);
        seedParams.seed = seed;
        seedParams.start_tick = start_tick;
        seedParams.input_delay = input_delay;
        seedParams.role = role;
    }

    sock = socket;
    connected = true;
    // HELLO/SEED 핸드셰이크 생략 — 릴레이가 MATCH_FOUND 로 seed/role 을 이미 확정
    lastPongMs.store(now_ms());
    lastPingSentMs.store(0);
    ready = true;
    th = std::thread(&Session::ioThread, this);
    std::cout << "[NET] Adopted relay socket: role="
              << (role == Role::Host ? "HOST" : "GUEST")
              << " seed=0x" << std::hex << seed << std::dec << std::endl;
    return true;
}

bool Session::Connect(const std::string& host, uint16_t port) {
    std::cout << "[NET] Connecting to " << host << ":" << port << std::endl;

    // Close() 이후 재사용을 위한 상태 리셋 (sendQ / HASH 포함)
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    listening = false;
    {
        std::lock_guard<std::mutex> lk(inMu);
        remoteInputs.clear();
    }
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }

    sock = tcp_connect(host, port);
    if (!sock.valid()) {
        std::cout << "[NET] Failed to connect to " << host << ":" << port << std::endl;
        connectionFailed = true;
        return false;
    }
    std::cout << "[NET] Connected to " << host << ":" << port << std::endl;
    connected = true;
    th = std::thread(&Session::ioThread, this);
    {
        std::vector<uint8_t> pl; le_write_u16(pl, 1);
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Sent HELLO message" << std::endl;
    }
    return true;
}

void Session::SendInput(uint32_t tick, uint8_t mask) {
    // 메인 스레드 활성 시각 갱신 — ioThread 의 스톨 감지 (창 드래그 대응) 이 이 값
    // 을 기준으로 동작한다.
    lastMainActivityMs_.store(now_ms());
    auto cur = lastLocalTick.load();
    if (tick > cur) lastLocalTick.store(tick);
    std::vector<uint8_t> pl; le_write_u32(pl, tick); le_write_u16(pl, 1); pl.push_back(mask);
    auto fr = build_frame(MsgType::INPUT, pl);
    std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
}

void Session::SendHash(uint32_t tick, uint64_t hash) {
    std::vector<uint8_t> pl; le_write_u32(pl, tick); le_write_u64(pl, hash);
    auto fr = build_frame(MsgType::HASH, pl);
    std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
}

void Session::SendGameOverChoice(GameOverChoice choice) {
    localGameOverChoice.store((uint8_t)choice);
    std::vector<uint8_t> pl;
    pl.push_back((uint8_t)choice);
    auto fr = build_frame(MsgType::GAME_OVER_CHOICE, pl);
    std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    std::cout << "[NET] Sent game over choice: " << (int)choice << std::endl;
}

void Session::SendNewSeed(uint64_t newSeed) {
    std::vector<uint8_t> pl;
    {
        std::lock_guard<std::mutex> lk(seedMu);
        seedParams.seed = newSeed;
        le_write_u64(pl, seedParams.seed);
        le_write_u32(pl, seedParams.start_tick);
        pl.push_back(seedParams.input_delay);
        pl.push_back((uint8_t)seedParams.role);
    }
    auto fr = build_frame(MsgType::SEED, pl);
    std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    std::cout << "[NET] Sent new seed: 0x" << std::hex << newSeed << std::dec << std::endl;
}

void Session::SendChat(const std::string& text) {
    // 길이 상한 — 프레임 페이로드 한도(MAX_PAYLOAD_BYTES=4096)보다 훨씬 작게 클램프.
    // UTF-8 을 자르면 부분 바이트가 될 수 있으므로 호출부에서 이미 200자 이내로
    // 잘라놓는 것이 원칙. 여기서는 최종 방어만.
    constexpr size_t kMax = 1024;
    size_t n = text.size() > kMax ? kMax : text.size();
    std::vector<uint8_t> pl;
    le_write_u16(pl, (uint16_t)n);
    pl.insert(pl.end(), text.begin(), text.begin() + n);
    auto fr = build_frame(MsgType::CHAT, pl);
    std::lock_guard<std::mutex> lk(sendMu);
    sendQ.push_back(std::move(fr));
}

bool Session::PullChat(std::string& outText) {
    std::lock_guard<std::mutex> lk(chatMu_);
    if (chatQ_.empty()) return false;
    outText = std::move(chatQ_.front());
    chatQ_.pop_front();
    return true;
}

void Session::SendMatchSummary(uint8_t won,
                               uint32_t my_score, uint32_t my_lines,
                               uint32_t opp_score, uint32_t opp_lines,
                               uint32_t duration_s) {
    // 페이로드: [won:1][my_score:4][my_lines:4][opp_score:4][opp_lines:4][duration:4] (21B)
    std::vector<uint8_t> pl;
    pl.reserve(21);
    pl.push_back(won ? 1 : 0);
    le_write_u32(pl, my_score);
    le_write_u32(pl, my_lines);
    le_write_u32(pl, opp_score);
    le_write_u32(pl, opp_lines);
    le_write_u32(pl, duration_s);
    auto fr = build_frame(MsgType::MATCH_SUMMARY, pl);
    std::lock_guard<std::mutex> lk(sendMu);
    sendQ.push_back(std::move(fr));
}

bool Session::GetMatchResult(MatchResult& out) const {
    std::lock_guard<std::mutex> lk(matchResultMu_);
    if (!matchResultValid_) return false;
    out = matchResult_;
    return true;
}

bool Session::GetRemoteInput(uint32_t tick, uint8_t& outMask) {
    std::lock_guard<std::mutex> lk(inMu);
    auto it = remoteInputs.find(tick);
    if (it == remoteInputs.end()) return false;
    outMask = it->second; return true;
}

void Session::Close() {
    quit = true;
    // 소켓을 먼저 닫아야 accept()/recv()에서 블로킹 중인 스레드가 해제됨
    if (listening && listenSock.valid()) tcp_close(listenSock);
    if (sock.valid()) tcp_close(sock);
    // 소켓 닫힌 후 스레드 join (블로킹 해제됨)
    if (ath.joinable()) ath.join();
    if (qth.joinable()) qth.join();
    if (rth.joinable()) rth.join();
    if (th.joinable()) th.join();
    // tcp_connect 중이던 queueThread/roomThread 가 Close 후 sock 을 뒤늦게 할당했을
    // 가능성이 있으므로 join 후 한 번 더 닫는다. (누수 방지)
    if (sock.valid()) tcp_close(sock);
    if (listenSock.valid()) tcp_close(listenSock);
    connected = false; ready = false; listening = false;
    roomState_.store(RoomState::Idle);
    roomPeerCount_.store(0);
    {
        std::lock_guard<std::mutex> lk(roomMu_);
        roomCode_.clear();
    }
    {
        std::lock_guard<std::mutex> lk(roomSendMu_);
        roomSendQ_.clear();
    }
    queueMatched_.store(false);
    queueLocalReady_.store(false);
    queuePeerReady_.store(false);
    {
        std::lock_guard<std::mutex> lk(queueSendMu_);
        queueSendQ_.clear();
    }
    // 스톨 heartbeat 상태 초기화 — 다음 세션에서 첫 SendInput 까지는 비활성.
    lastMainActivityMs_.store(0);
    heartbeatTickEnd_.store(0);
    lastHeartbeatMs_ = 0;
    {
        std::lock_guard<std::mutex> lk(chatMu_);
        chatQ_.clear();
    }
    // 게임 sendQ / HASH pair 도 함께 비움 — 같은 Session 객체 재사용 시 이전
    // 연결의 stale 프레임이 새 연결의 ioThread 에서 선두로 나가는 것 방지.
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }
    // MATCH_RESULT 도 초기화. ClearGameOverChoices 만 의존하면 타이틀→새 매치
    // 경로에서 이전 라운드 결과가 새 매치 게임오버 시점에 즉시 읽히는 경계가
    // 있었다. Close 는 세션 경계마다 반드시 실행되므로 여기서 보장.
    {
        std::lock_guard<std::mutex> lk(matchResultMu_);
        matchResultValid_ = false;
        matchResult_ = MatchResult{};
    }
}

bool Session::QueueJoin(const std::string& host, uint16_t port,
                        uint32_t start_tick, uint8_t input_delay,
                        const std::string& auth_token) {
    if (qth.joinable() || th.joinable() || ath.joinable()) return false;

    // Close() 이후 재사용을 위한 상태 리셋 (sendQ / HASH 포함)
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    listening = false;
    {
        std::lock_guard<std::mutex> lk(inMu);
        remoteInputs.clear();
    }
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }
    queueMatched_.store(false);
    queueLocalReady_.store(false);
    queuePeerReady_.store(false);
    { std::lock_guard<std::mutex> lk(queueSendMu_); queueSendQ_.clear(); }

    qth = std::thread(&Session::queueThread, this, host, port, start_tick, input_delay, auth_token);
    return true;
}

void Session::QueueCancel() {
    // 큐잉 중에만 호출 — 가능하면 릴레이에 명시 취소를 먼저 보낸 뒤,
    // sock 을 닫아 recv 블록을 해제하고 quit 로 루프 종료.
    if (sock.valid()) {
        auto fr = build_frame(MsgType::QUEUE_CANCEL, {});
        if (!fr.empty()) {
            tcp_send_all(sock, fr.data(), fr.size());
        }
    }
    quit = true;
    if (sock.valid()) tcp_close(sock);
    // qth.join() 은 Close() 에서 처리 — 여기선 블록 없이 신호만 보낸다.
}

void Session::QueueConfirm() {
    // 로비에서 "수락" 버튼 — READY(1) 을 queueThread outbound 큐에 적재.
    // 실제 전송은 queueThread 가 처리하고, peer READY(1) 까지 오면 ioThread 로 전환.
    if (queueLocalReady_.exchange(true)) return;  // idempotent
    std::vector<uint8_t> pl; pl.push_back(1);
    auto fr = build_frame(MsgType::READY, pl);
    std::lock_guard<std::mutex> lk(queueSendMu_);
    queueSendQ_.push_back(std::move(fr));
}

void Session::QueueDecline() {
    // 로비에서 "거절". READY(0) 을 동기적으로 송신한 뒤 quit 을 세팅한다.
    //   이전 구현은 queueSendQ_ 에 밀어넣고 quit=true 를 즉시 세팅했지만 —
    //   queueThread 는 while(!quit) 상단에서 quit 을 먼저 보고 drain 없이 바로
    //   종료, main.cpp 가 곧바로 Close() 로 소켓을 닫아 READY(0) 이 실제로
    //   송신되지 않고 relay 쪽은 EOF 로만 본다. 그 결과 상대는 "거절" 이 아니라
    //   "상대 timeout/EOF" 로 판정받는 경계 케이스가 있었다.
    //   여기서 직접 tcp_send_all 을 호출하되, queueThread drain 과 같은 fd 에
    //   interleaved 쓰기가 되지 않도록 queueSockSendMu_ 로 직렬화.
    std::vector<uint8_t> pl; pl.push_back(0);
    auto fr = build_frame(MsgType::READY, pl);
    if (sock.valid()) {
        std::lock_guard<std::mutex> lk(queueSockSendMu_);
        tcp_send_all(sock, fr.data(), fr.size());
    }
    quit = true;
}

bool Session::RoomCreate(const std::string& host, uint16_t port,
                         uint32_t start_tick, uint8_t input_delay,
                         const std::string& auth_token) {
    if (qth.joinable() || th.joinable() || ath.joinable() || rth.joinable()) return false;
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    listening = false;
    { std::lock_guard<std::mutex> lk(inMu); remoteInputs.clear(); }
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }
    roomState_.store(RoomState::Connecting);
    roomPeerCount_.store(0);
    { std::lock_guard<std::mutex> lk(roomMu_); roomCode_.clear(); }
    { std::lock_guard<std::mutex> lk(roomSendMu_); roomSendQ_.clear(); }
    rth = std::thread(&Session::roomThread, this, host, port,
                      std::string{}, start_tick, input_delay, auth_token);
    return true;
}

bool Session::RoomJoin(const std::string& host, uint16_t port,
                       const std::string& code,
                       uint32_t start_tick, uint8_t input_delay,
                       const std::string& auth_token) {
    if (qth.joinable() || th.joinable() || ath.joinable() || rth.joinable()) return false;
    if (code.empty() || code.size() > 255) return false;
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    listening = false;
    { std::lock_guard<std::mutex> lk(inMu); remoteInputs.clear(); }
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }
    roomState_.store(RoomState::Connecting);
    roomPeerCount_.store(0);
    { std::lock_guard<std::mutex> lk(roomMu_); roomCode_ = code; }
    { std::lock_guard<std::mutex> lk(roomSendMu_); roomSendQ_.clear(); }
    rth = std::thread(&Session::roomThread, this, host, port,
                      code, start_tick, input_delay, auth_token);
    return true;
}

void Session::RoomSendReady(bool readyFlag) {
    std::vector<uint8_t> pl; pl.push_back(readyFlag ? 1 : 0);
    auto fr = build_frame(MsgType::READY, pl);
    std::lock_guard<std::mutex> lk(roomSendMu_);
    roomSendQ_.push_back(std::move(fr));
}

void Session::RoomLeave() {
    // ROOM_LEAVE 는 동기적으로 직접 송신한다 — 이전 구현은 roomSendQ_ 에 넣고
    // quit=true 를 즉시 세팅했지만, roomThread 루프가 while(!quit) 상단에서
    // quit 을 먼저 보고 drain 없이 종료, main.cpp 는 곧바로 Close() 로 소켓을
    // 닫아 ROOM_LEAVE 가 실제로 송신되지 않는 경계가 있었다. (QueueDecline 과
    // 동일한 패턴 — roomSockSendMu_ 로 roomThread drain 과의 interleave 방지.)
    auto fr = build_frame(MsgType::ROOM_LEAVE, {});
    if (sock.valid()) {
        std::lock_guard<std::mutex> lk(roomSockSendMu_);
        tcp_send_all(sock, fr.data(), fr.size());
    }
    quit = true;
}

void Session::roomThread(std::string host, uint16_t port,
                         std::string joinCode,
                         uint32_t start_tick, uint8_t input_delay,
                         std::string auth_token) {
    const bool doCreate = joinCode.empty();
    std::cout << "[ROOM] Connecting to relay " << host << ":" << port
              << " for " << (doCreate ? "CREATE" : ("JOIN " + joinCode)) << std::endl;
    TcpSocket s = tcp_connect(host, port);
    if (!s.valid()) {
        std::cout << "[ROOM] Failed to connect" << std::endl;
        roomState_.store(RoomState::Failed);
        connectionFailed = true;
        return;
    }
    // connect 중에 Close()가 호출됐다면 sock 할당 전에 로컬에서 닫고 빠져나간다.
    // 그렇지 않으면 Close 의 sock.valid() 체크가 이미 지나간 뒤 할당되어 fd 누수.
    if (quit.load()) {
        tcp_close(s);
        roomState_.store(RoomState::Idle);
        return;
    }
    sock = s;
    connected = true;

    // 첫 프레임 송신. 페이로드 끝에 [tok_len:1][token:N] 추가.
    // 토큰 길이는 최대 255 로 clamp — 실제로는 32 hex chars 표준.
    auto append_token = [&](std::vector<uint8_t>& pl) {
        const size_t n = std::min<size_t>(auth_token.size(), 255);
        pl.push_back(static_cast<uint8_t>(n));
        for (size_t i = 0; i < n; ++i) pl.push_back(static_cast<uint8_t>(auth_token[i]));
    };
    std::vector<uint8_t> first;
    if (doCreate) {
        std::vector<uint8_t> pl;
        append_token(pl);
        first = build_frame(MsgType::ROOM_CREATE, pl);
    } else {
        std::vector<uint8_t> pl;
        pl.push_back(static_cast<uint8_t>(joinCode.size()));
        for (char c : joinCode) pl.push_back(static_cast<uint8_t>(c));
        append_token(pl);
        first = build_frame(MsgType::ROOM_JOIN, pl);
    }
    if (!tcp_send_all(sock, first.data(), first.size())) {
        std::cout << "[ROOM] Failed to send first frame" << std::endl;
        roomState_.store(RoomState::Failed);
        connectionFailed = true;
        quit = true;
        return;
    }

    std::vector<uint8_t> buf;
    while (!quit.load()) {
        // 아웃바운드 drain (READY) — ROOM_LEAVE 는 RoomLeave() 가 동기 송신.
        // roomSockSendMu_ 로 RoomLeave 의 직접 송신과 직렬화.
        while (true) {
            std::vector<uint8_t> pkt;
            {
                std::lock_guard<std::mutex> lk(roomSendMu_);
                if (roomSendQ_.empty()) break;
                pkt = std::move(roomSendQ_.front());
                roomSendQ_.pop_front();
            }
            std::lock_guard<std::mutex> lkSock(roomSockSendMu_);
            if (!tcp_send_all(sock, pkt.data(), pkt.size())) {
                std::cout << "[ROOM] Send failed" << std::endl;
                roomState_.store(RoomState::Failed);
                connectionFailed = true;
                quit = true;
                break;
            }
        }

        if (quit.load()) break;

        if (!tcp_recv_some(sock, buf)) {
            std::cout << "[ROOM] Disconnected" << std::endl;
            roomState_.store(RoomState::Failed);
            connectionFailed = true;
            quit = true;
            break;
        }

        std::vector<Frame> frames;
        parse_frames(buf, frames);
        bool matchFound = false;
        for (auto& f : frames) {
            if (f.type == MsgType::ROOM_INFO) {
                // [code_len:1][code:N][status:1][peer_count:1]
                if (f.payload.size() < 3) continue;
                uint8_t n = f.payload[0];
                if (f.payload.size() < 1u + n + 2u) continue;
                std::string code(f.payload.begin() + 1, f.payload.begin() + 1 + n);
                uint8_t status = f.payload[1 + n];
                uint8_t peerCount = f.payload[2 + n];
                {
                    std::lock_guard<std::mutex> lk(roomMu_);
                    roomCode_ = code;
                }
                roomPeerCount_.store(peerCount);
                switch (status) {
                    case 0: roomState_.store(peerCount >= 2 ? RoomState::WaitingWithPeer
                                                            : RoomState::Waiting); break;
                    case 1: roomState_.store(RoomState::Full); break;
                    case 2: roomState_.store(RoomState::NotFound); break;
                    case 3: roomState_.store(RoomState::GoneFull); break;
                    default: break;
                }
                std::cout << "[ROOM] INFO code=" << code
                          << " status=" << (int)status
                          << " peers=" << (int)peerCount << std::endl;
                // NotFound/Full: 서버가 소켓을 닫을 예정이라 이 스레드도 곧 EOF로 종료된다.
            } else if (f.type == MsgType::READY) {
                // 상대방의 READY 에코 — UI 표시용으로만 사용 (세션에 저장 안 함).
                // peerReady 상태는 main.cpp 쪽에서 별도 플래그로 추적할 수 있도록 로그만.
                std::cout << "[ROOM] peer READY="
                          << (f.payload.empty() ? 0 : (int)f.payload[0]) << std::endl;
            } else if (f.type == MsgType::MATCH_FOUND && f.payload.size() >= 9) {
                uint8_t roleByte = f.payload[0];
                uint64_t seed = le_read_u64(f.payload.data() + 1);
                Role role = (roleByte == (uint8_t)Role::Host) ? Role::Host : Role::Peer;
                {
                    std::lock_guard<std::mutex> lk(seedMu);
                    seedParams.seed = seed;
                    seedParams.start_tick = start_tick;
                    seedParams.input_delay = input_delay;
                    seedParams.role = role;
                }
                std::cout << "[ROOM] MATCH_FOUND role="
                          << (role == Role::Host ? "HOST" : "GUEST")
                          << " seed=0x" << std::hex << seed << std::dec << std::endl;
                matchFound = true;
                // 계속 루프를 돌며 남은 frames 를 검사 — 릴레이가 MATCH_FOUND 직후
                // 게임 포워딩을 시작하므로 같은 recv 에 실린 게임 프레임을 놓치지
                // 않도록 아래 else 브랜치에서 recvBuf 에 복원한다.
            } else if (matchFound) {
                // MATCH_FOUND 가 먼저 온 뒤 같은 recv 에 실린 게임 프레임 (INPUT/PING 등).
                // 버리면 lockstep 1 tick stall 또는 첫 PING 유실. 재직렬화해 recvBuf
                // 맨 뒤에 쌓는다 — ioThread 가 첫 루프에서 parse_frames 로 소비.
                auto bytes = build_frame(f.type, f.payload);
                recvBuf.insert(recvBuf.end(), bytes.begin(), bytes.end());
            }
            // 그 외 (matchFound 이전의 예기치 못한 프레임)는 로비 단계라 관심 없음.
        }
        if (matchFound) {
            // parse_frames 가 뜯어내고 남은 incomplete-tail 바이트도 그대로 이관.
            recvBuf.insert(recvBuf.end(), buf.begin(), buf.end());
            lastPongMs.store(now_ms());
            lastPingSentMs.store(0);
            roomState_.store(RoomState::Starting);
            ready = true;
            th = std::thread(&Session::ioThread, this);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // quit 에 의해 종료된 경로 — 소켓 정리
    std::cout << "[ROOM] Leaving / cancelled" << std::endl;
    if (sock.valid()) {
        tcp_close(sock);
        sock = TcpSocket{};
    }
    connected = false;
    if (roomState_.load() != RoomState::Starting) {
        roomState_.store(RoomState::Idle);
    }
}

void Session::queueThread(std::string host, uint16_t port,
                          uint32_t start_tick, uint8_t input_delay,
                          std::string auth_token) {
    std::cout << "[QUEUE] Connecting to relay " << host << ":" << port << std::endl;
    TcpSocket s = tcp_connect(host, port);
    if (!s.valid()) {
        std::cout << "[QUEUE] Failed to connect to relay" << std::endl;
        connectionFailed = true;
        return;
    }
    // connect 중에 QueueCancel/Close 가 호출됐다면 sock 할당 전에 로컬에서 닫는다.
    if (quit.load()) {
        tcp_close(s);
        return;
    }
    sock = s;
    connected = true;
    std::cout << "[QUEUE] Connected, sending QUEUE_JOIN" << std::endl;

    // QUEUE_JOIN 페이로드: [tok_len:1][token:N]
    std::vector<uint8_t> joinPl;
    {
        const size_t n = std::min<size_t>(auth_token.size(), 255);
        joinPl.push_back(static_cast<uint8_t>(n));
        for (size_t i = 0; i < n; ++i) joinPl.push_back(static_cast<uint8_t>(auth_token[i]));
    }
    auto join = build_frame(MsgType::QUEUE_JOIN, joinPl);
    if (!tcp_send_all(sock, join.data(), join.size())) {
        std::cout << "[QUEUE] Failed to send QUEUE_JOIN" << std::endl;
        connectionFailed = true; quit = true;
        return;
    }

    // MATCH_FOUND 대기 — 최대 5분, 2ms 폴링.
    auto matchDeadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    std::vector<uint8_t> buf;
    bool matched = false;
    while (!quit.load() && !matched) {
        if (std::chrono::steady_clock::now() >= matchDeadline) {
            std::cout << "[QUEUE] Timeout waiting for MATCH_FOUND" << std::endl;
            connectionFailed = true; quit = true;
            return;
        }
        if (!tcp_recv_some(sock, buf)) {
            std::cout << "[QUEUE] Relay disconnected before MATCH_FOUND" << std::endl;
            connectionFailed = true; quit = true;
            return;
        }
        std::vector<Frame> frames;
        parse_frames(buf, frames);
        // MATCH_FOUND 뒤에 같은 recv 에 실린 프레임을 다음 단계(로비)로 넘기기 위한 보존 버퍼.
        // build_frame 은 동일 payload 에 대해 bit-identical 재생산되므로 체크섬 포함 복원 가능.
        std::vector<uint8_t> preserve;
        for (auto& f : frames) {
            if (f.type == MsgType::MATCH_FOUND && f.payload.size() >= 9) {
                uint8_t roleByte = f.payload[0];
                uint64_t seed = le_read_u64(f.payload.data() + 1);
                Role role = (roleByte == (uint8_t)Role::Host) ? Role::Host : Role::Peer;
                {
                    std::lock_guard<std::mutex> lk(seedMu);
                    seedParams.seed = seed;
                    seedParams.start_tick = start_tick;
                    seedParams.input_delay = input_delay;
                    seedParams.role = role;
                }
                std::cout << "[QUEUE] MATCH_FOUND role="
                          << (role == Role::Host ? "HOST" : "GUEST")
                          << " seed=0x" << std::hex << seed << std::dec
                          << " — waiting for user to accept..." << std::endl;
                matched = true;
                queueMatched_.store(true);
            } else if (matched) {
                // MATCH_FOUND 직후 같은 recv 에 실린 lobby/게임 프레임 (상대의 빠른
                // READY 또는 이미 포워딩 시작된 바이트). 재직렬화해 보존.
                auto bytes = build_frame(f.type, f.payload);
                preserve.insert(preserve.end(), bytes.begin(), bytes.end());
            }
        }
        // preserve + buf(partial tail) 순서로 합쳐야 스트림 시간 순서가 보존된다.
        // buf.insert(end) 는 partial tail 뒤에 붙여 다음 parse 때 오프셋이 어긋나므로 금지.
        if (!preserve.empty()) {
            preserve.insert(preserve.end(), buf.begin(), buf.end());
            buf = std::move(preserve);
        }
        if (!matched) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (!matched) {
        std::cout << "[QUEUE] Cancelled" << std::endl;
        return;
    }

    // 수락 로비 단계: 서버가 양쪽 READY(1) 을 수집할 때까지 대기.
    // · outbound: QueueConfirm/QueueDecline 이 queueSendQ_ 에 적재한 READY 프레임 drain.
    // · inbound : 릴레이가 포워딩한 peer 의 READY 수신. READY(1) → queuePeerReady_=true,
    //             READY(0) → 상대가 거절 → connectionFailed.
    //   양쪽 ready 가 되면 릴레이가 바로 게임 바이트 포워딩을 시작하므로, 여기서도
    //   ready=true 로 전환해 ioThread 기동.
    auto lobbyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    while (!quit.load()) {
        if (std::chrono::steady_clock::now() >= lobbyDeadline) {
            std::cout << "[QUEUE] Lobby timeout (peer did not accept)" << std::endl;
            connectionFailed = true; quit = true;
            return;
        }

        // outbound drain — QueueConfirm 결과 (QueueDecline 은 동기 송신 후 quit).
        // tcp_send_all 은 main thread 의 QueueDecline 과 같은 fd 로 동시 진입
        // 가능하므로 queueSockSendMu_ 로 직렬화.
        while (true) {
            std::vector<uint8_t> pkt;
            {
                std::lock_guard<std::mutex> lk(queueSendMu_);
                if (queueSendQ_.empty()) break;
                pkt = std::move(queueSendQ_.front());
                queueSendQ_.pop_front();
            }
            std::lock_guard<std::mutex> lkSock(queueSockSendMu_);
            if (!tcp_send_all(sock, pkt.data(), pkt.size())) {
                std::cout << "[QUEUE] Lobby send failed" << std::endl;
                connectionFailed = true; quit = true; break;
            }
        }
        if (quit.load()) break;

        if (!tcp_recv_some(sock, buf)) {
            std::cout << "[QUEUE] Lobby: peer/relay disconnected" << std::endl;
            connectionFailed = true; quit = true;
            return;
        }
        std::vector<Frame> frames;
        parse_frames(buf, frames);
        bool peerDeclined = false;
        // 로비 외 프레임(INPUT/PING/HASH 등)은 재직렬화해 recvBuf 에 바로 적재한다.
        // 릴레이는 양쪽 READY 를 본 순간부터 게임 바이트 포워딩을 시작하므로, 상대
        // ioThread 가 먼저 보낸 프레임이 같은 recv 에 묶여 로비 단계 queueThread
        // 로 들어올 수 있다. 버리면 첫 PING/INPUT 유실 → lockstep stall.
        for (auto& f : frames) {
            if (f.type == MsgType::READY) {
                uint8_t v = f.payload.empty() ? 0 : f.payload[0];
                if (v == 0) {
                    std::cout << "[QUEUE] Peer declined" << std::endl;
                    peerDeclined = true;
                } else {
                    queuePeerReady_.store(true);
                }
            } else {
                // 게임/기타 프레임 — 재직렬화해 recvBuf 로 이관(ioThread 가 소비).
                auto bytes = build_frame(f.type, f.payload);
                recvBuf.insert(recvBuf.end(), bytes.begin(), bytes.end());
            }
        }
        if (peerDeclined) {
            connectionFailed = true; quit = true;
            return;
        }

        if (queueLocalReady_.load() && queuePeerReady_.load()) {
            // 양쪽 수락 완료 → 게임 세션으로 전환.
            // parse_frames 가 뜯어내고 남은 partial tail 도 recvBuf 뒤에 붙여
            // ioThread 첫 루프에서 이어서 파싱되게 한다. (이미 보존된
            // 완성 프레임이 앞에 있고, 그 뒤에 partial 이 붙는 순서 → 스트림
            // 시간 순서 보존.)
            std::cout << "[QUEUE] Both accepted, starting game session" << std::endl;
            recvBuf.insert(recvBuf.end(), buf.begin(), buf.end());
            queueMatched_.store(false);
            lastPongMs.store(now_ms());
            lastPingSentMs.store(0);
            ready = true;
            th = std::thread(&Session::ioThread, this);
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // quit 로 나옴 (QueueDecline/QueueCancel/Close).
    std::cout << "[QUEUE] Lobby cancelled" << std::endl;
}

void Session::ioThread() {
    std::cout << "[NET] I/O thread started" << std::endl;
    auto startTime = std::chrono::steady_clock::now();
    const auto CONNECTION_TIMEOUT = std::chrono::seconds(10);

    while (!quit.load()) {
        bool hasActivity = false;

        if (!ready.load()) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > CONNECTION_TIMEOUT) {
                std::cout << "[NET] Connection timeout after 10 seconds" << std::endl;
                connectionFailed = true;
                quit = true;
                break;
            }
        }

        // 1Hz PING 송신 — ready=true 이후에만. 상대가 얼어붙어도 여기선 계속
        // 큐에 쌓이지만 tcp_send_all 자체가 막히지는 않는다(커널 버퍼 여유 범위).
        if (ready.load()) {
            int64_t now = now_ms();
            int64_t lastSent = lastPingSentMs.load();
            if (lastSent == 0 || (now - lastSent) >= 1000) {
                lastPingSentMs.store(now);
                std::vector<uint8_t> pl; le_write_u64(pl, (uint64_t)now);
                auto fr = build_frame(MsgType::PING, pl);
                std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
            }

            // 메인 스레드 스톨 자동 heartbeat — 창 드래그 시 메인 루프가 WM_ENTERSIZEMOVE
            // 모달에 갇혀 SendInput 이 멈춰도, ioThread 는 계속 돌고 있으므로 이 쪽에서
            // INPUT(tick,0) 을 대신 송신해 lockstep 을 계속 진행시킨다.
            //   · lastMainActivityMs_ == 0  → 첫 입력 전 (게임 시작 전) 이라 건너뜀.
            //   · 스톨 기준: 300ms 이상 SendInput 없음. 일반 60Hz 틱 (=16ms) 에선 트리거 안 됨.
            //   · 전송 주기: 16ms (60Hz) — 실제 게임 틱과 동일 속도로 catch-up.
            int64_t mainAct = lastMainActivityMs_.load();
            if (mainAct > 0 && (now - mainAct) > 300) {
                if (lastHeartbeatMs_ == 0 || (now - lastHeartbeatMs_) >= 16) {
                    lastHeartbeatMs_ = now;
                    uint32_t nextTick = lastLocalTick.load() + 1;
                    std::vector<uint8_t> pl;
                    le_write_u32(pl, nextTick);
                    le_write_u16(pl, 1);
                    pl.push_back(0);
                    auto fr = build_frame(MsgType::INPUT, pl);
                    lastLocalTick.store(nextTick);
                    heartbeatTickEnd_.store(nextTick);
                    std::lock_guard<std::mutex> lk(sendMu);
                    sendQ.push_back(std::move(fr));
                }
            } else {
                lastHeartbeatMs_ = 0;
            }
        }

        size_t prevSize = recvBuf.size();
        if (tcp_recv_some(sock, recvBuf)) {
            const bool newBytes = (recvBuf.size() > prevSize);
            if (newBytes) hasActivity = true;
            // 주의: 이 루프는 60Hz+ 로 돈다. 여기서 std::cout 으로 매 recv/parse
            // 를 찍으면 Windows 콘솔 I/O 가 blocking 해 Host 쪽 프레임이 밀린다.
            // 로그가 필요하면 NET_TRACE 매크로 등으로 gate 해 debug 빌드에서만.
            //
            // 조건은 newBytes 뿐 아니라 "recvBuf 가 비어있지 않은 경우" 로 확장한다.
            // queueThread 로비 / roomThread MATCH_FOUND 분기가 ioThread 전환 시
            // 재직렬화된 프레임을 recvBuf 에 pre-load 해 두는데, 첫 recv 가 0 바이트
            // 를 리턴하면 parse_frames 자체가 스킵되어 preload 가 소비되지 않는다.
            if (newBytes || !recvBuf.empty()) {
                std::vector<Frame> frames;
                parse_frames(recvBuf, frames);
                for (auto& f : frames) handleFrame(f);
            }
        } else {
            std::cout << "[NET] Connection lost or receive failed" << std::endl;
            connectionFailed = true;
            quit = true;
            break;
        }

        while (true) {
            std::vector<uint8_t> pkt;
            {
                std::lock_guard<std::mutex> lk(sendMu);
                if (sendQ.empty()) break;
                pkt = std::move(sendQ.front());
                sendQ.pop_front();
                hasActivity = true;
            }
            // sendMu released before blocking I/O — main thread can SendInput() freely
            if (!tcp_send_all(sock, pkt.data(), pkt.size())) {
                std::cout << "[NET] Send failed!" << std::endl;
                quit = true;
                break;
            }
        }

        if (!hasActivity) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    std::cout << "[NET] I/O thread exiting" << std::endl;
}

void Session::acceptThread(uint16_t port)
{
    std::cout << "[NET] Starting to listen on port " << port << std::endl;
    listenSock = tcp_listen(port, 1);
    if (!listenSock.valid()) {
        std::cout << "[NET] Failed to listen on port " << port << std::endl;
        listening = false;
        return;
    }
    std::cout << "[NET] Listening on port " << port << ", waiting for connection..." << std::endl;
    auto client = tcp_accept(listenSock);
    tcp_close(listenSock);
    listenSock = TcpSocket{};
    if (!client.valid()) {
        std::cout << "[NET] Failed to accept client connection" << std::endl;
        listening = false;
        return;
    }
    // accept 중에 Close() 가 호출됐다면 (quit 세팅) 여기서 바로 닫고 빠져나간다.
    if (quit.load()) {
        tcp_close(client);
        listening = false;
        return;
    }
    std::cout << "[NET] Client connected!" << std::endl;
    sock = client;
    connected = true;
    listening = false;
    th = std::thread(&Session::ioThread, this);
    {
        std::vector<uint8_t> pl; le_write_u16(pl, 1);
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Queued HELLO message" << std::endl;
    }
    {
        std::vector<uint8_t> pl;
        {
            std::lock_guard<std::mutex> lk(seedMu);
            le_write_u64(pl, seedParams.seed);
            le_write_u32(pl, seedParams.start_tick);
            pl.push_back(seedParams.input_delay);
            pl.push_back((uint8_t)seedParams.role);
        }
        auto fr = build_frame(MsgType::SEED, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        {
            std::lock_guard<std::mutex> lk2(seedMu);
            std::cout << "[NET] Queued SEED message (seed=0x" << std::hex << seedParams.seed << std::dec << ")" << std::endl;
        }
    }
    lastPongMs.store(now_ms());
    lastPingSentMs.store(0);
    ready = true;
    std::cout << "[NET] Host session is ready!" << std::endl;
}

void Session::handleFrame(const Frame& f) {
    switch (f.type) {
    case MsgType::HELLO: {
        std::cout << "[NET] Received HELLO message" << std::endl;
        std::vector<uint8_t> pl; pl.push_back(1);
        auto fr = build_frame(MsgType::HELLO_ACK, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Queued HELLO_ACK response" << std::endl;
    } break;
    case MsgType::HELLO_ACK: {
        std::cout << "[NET] Received HELLO_ACK message" << std::endl;
    } break;
    case MsgType::SEED: {
        std::cout << "[NET] Received SEED message" << std::endl;
        if (f.payload.size() >= 8+4+1+1) {
            const uint8_t* p = f.payload.data();
            std::lock_guard<std::mutex> lk(seedMu);
            seedParams.seed = le_read_u64(p);
            seedParams.start_tick = le_read_u32(p+8);
            seedParams.input_delay = p[12];
            uint8_t rawRole = p[13];
            seedParams.role = (rawRole == (uint8_t)Role::Host || rawRole == (uint8_t)Role::Peer)
                            ? (Role)rawRole : Role::Peer;
            std::cout << "[NET] Parsed SEED: seed=0x" << std::hex << seedParams.seed
                      << ", start_tick=" << std::dec << seedParams.start_tick
                      << ", input_delay=" << (int)seedParams.input_delay << std::endl;
            lastPongMs.store(now_ms());
            lastPingSentMs.store(0);
            ready = true;
            std::cout << "[NET] Client session is ready!" << std::endl;
        } else {
            std::cout << "[NET] Invalid SEED message size: " << f.payload.size() << std::endl;
        }
    } break;
    case MsgType::INPUT: {
        if (f.payload.size() >= 6) {
            const uint8_t* p = f.payload.data();
            uint32_t from = le_read_u32(p);
            uint16_t cnt = le_read_u16(p+4);
            // 페이로드 크기 검증: 헤더(6) + cnt 바이트가 실제 크기 이내인지 확인
            if (static_cast<size_t>(6) + cnt > f.payload.size()) break;
            const uint8_t* arr = p+6;
            for (uint16_t i=0;i<cnt;++i) {
                uint32_t tick = from + i;
                uint8_t m = arr[i];
                {
                    std::lock_guard<std::mutex> lk(inMu);
                    remoteInputs.emplace(tick, m);
                    if (tick > lastRemoteTick) lastRemoteTick = tick;
                }
            }
            std::vector<uint8_t> ack; le_write_u32(ack, lastRemoteTick.load());
            auto fr = build_frame(MsgType::ACK, ack);
            std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        }
    } break;
    case MsgType::ACK: {
    } break;
    case MsgType::HASH: {
        if (f.payload.size() == 4+8) {
            const uint8_t* p = f.payload.data();
            uint32_t t = le_read_u32(p);
            uint64_t h = le_read_u64(p+4);
            std::lock_guard<std::mutex> lk(hashMu_);
            lastHashTickRemote = t;
            lastHashRemote = h;
        }
    } break;
    case MsgType::GAME_OVER_CHOICE: {
        if (f.payload.size() >= 1) {
            uint8_t choice = f.payload[0];
            // enum 정의 밖 값은 무시 — 손상/악의 프레임 방어.
            if (choice == (uint8_t)GameOverChoice::Restart ||
                choice == (uint8_t)GameOverChoice::GoToTitle) {
                remoteGameOverChoice.store(choice);
                std::cout << "[NET] Received game over choice: " << (int)choice << std::endl;
            } else {
                std::cout << "[NET] Dropping invalid game-over choice: " << (int)choice << std::endl;
            }
        }
    } break;
    case MsgType::PING: {
        // 상대의 PING 은 즉시 PONG 으로 에코 — io 스레드가 계속 돌고 있으면
        // 메인 스레드가 얼어도(창 드래그 등) 상대는 우리를 살아있다고 판정.
        std::vector<uint8_t> pong = f.payload; auto fr = build_frame(MsgType::PONG, pong);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    } break;
    case MsgType::PONG: {
        // 최신 PONG 도착 시각 기록 — linkStatus() 가 이 값을 기준으로 판정.
        lastPongMs.store(now_ms());
    } break;
    case MsgType::CHAT: {
        // [text_len:2][utf8:N]
        if (f.payload.size() < 2) break;
        uint16_t n = le_read_u16(f.payload.data());
        if ((size_t)n + 2 > f.payload.size()) break;  // 손상 — 드롭
        std::string text((const char*)f.payload.data() + 2, n);
        std::lock_guard<std::mutex> lk(chatMu_);
        chatQ_.push_back(std::move(text));
    } break;
    case MsgType::MATCH_RESULT: {
        // [elo_before:4 LE][elo_after:4 LE][delta:4 LE signed]  (12 bytes)
        if (f.payload.size() < 12) break;
        const uint8_t* p = f.payload.data();
        MatchResult r;
        r.elo_before = static_cast<int32_t>(le_read_u32(p));
        r.elo_after  = static_cast<int32_t>(le_read_u32(p + 4));
        r.delta      = static_cast<int32_t>(le_read_u32(p + 8));
        std::lock_guard<std::mutex> lk(matchResultMu_);
        matchResult_ = r;
        matchResultValid_ = true;
    } break;
    default: break;
    }
}

bool Session::GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const {
    std::lock_guard<std::mutex> lk(hashMu_);
    tick = lastHashTickRemote;
    hash = lastHashRemote;
    return tick != 0;
}

bool Session::GetRemoteGameOverChoice(GameOverChoice& outChoice) const {
    uint8_t val = remoteGameOverChoice.load();
    if (val == 0) return false;
    outChoice = (GameOverChoice)val;
    return true;
}

void Session::ClearGameOverChoices() {
    localGameOverChoice.store(0);
    remoteGameOverChoice.store(0);
    // 다음 라운드의 MATCH_RESULT 를 새로 받을 수 있도록 상태도 함께 리셋.
    {
        std::lock_guard<std::mutex> lk(matchResultMu_);
        matchResultValid_ = false;
        matchResult_ = MatchResult{};
    }
}

void Session::ClearInputs() {
    {
        std::lock_guard<std::mutex> lk(inMu);
        remoteInputs.clear();
        lastRemoteTick.store(0);
        lastLocalTick.store(0);
    }
    // 재시작 경계에서 outbound sendQ 에 남아있는 이전 라운드 INPUT/HASH 를 드롭.
    // 프로토콜에 round-id 가 없어 새 라운드의 tick 번호와 stale 이 섞이면 수신
    // 측 remoteInputs.emplace 가 stale 을 선점해 DESYNC 를 유발할 수 있다.
    //
    // 중요: 모두 비우면 안 된다. SendNewSeed() 같은 컨트롤 프레임(SEED 등) 이
    // 아직 drain 되지 않았을 수 있다 — 네트워크 stall 상태에서 Host 가 restart
    // 를 누르고 1.5초 경과 후 ClearInputs 가 호출되면, 아직 송신되지 못한 SEED
    // 프레임까지 같이 날아가 Guest 가 WaitingForNewSeed 타임아웃으로 떨어진다.
    // 따라서 frame type 을 보고 INPUT/HASH 만 필터링해 드롭.
    // 프레임 레이아웃: [len:2][type:1][payload:N][chk:4] → byte[2] == MsgType.
    {
        std::lock_guard<std::mutex> lk(sendMu);
        std::deque<std::vector<uint8_t>> keep;
        for (auto& fr : sendQ) {
            if (fr.size() < 3) continue;  // malformed
            MsgType t = (MsgType)fr[2];
            if (t == MsgType::INPUT || t == MsgType::HASH) continue;  // 드롭
            keep.push_back(std::move(fr));
        }
        sendQ = std::move(keep);
    }
    // 원격 HASH 도 초기화 — 이전 라운드 hash 가 새 라운드 tick 과 충돌 방지.
    {
        std::lock_guard<std::mutex> lk(hashMu_);
        lastHashTickRemote = 0;
        lastHashRemote = 0;
    }
    // 재시작 경계에서 heartbeat 상태도 리셋 — 새 라운드의 tick 0 부터 다시 감지.
    lastMainActivityMs_.store(0);
    heartbeatTickEnd_.store(0);
    lastHeartbeatMs_ = 0;
    std::cout << "[NET] Cleared input queues for restart" << std::endl;
}

}
