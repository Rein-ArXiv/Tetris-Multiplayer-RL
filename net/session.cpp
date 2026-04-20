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
    {
        std::lock_guard<std::mutex> lk(chatMu_);
        chatQ_.clear();
    }
    // 게임 sendQ / HASH pair 도 함께 비움 — 같은 Session 객체 재사용 시 이전
    // 연결의 stale 프레임이 새 연결의 ioThread 에서 선두로 나가는 것 방지.
    { std::lock_guard<std::mutex> lk(sendMu); sendQ.clear(); }
    { std::lock_guard<std::mutex> lk(hashMu_); lastHashTickRemote = 0; lastHashRemote = 0; }
}

bool Session::QueueJoin(const std::string& host, uint16_t port,
                        uint32_t start_tick, uint8_t input_delay) {
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

    qth = std::thread(&Session::queueThread, this, host, port, start_tick, input_delay);
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

bool Session::RoomCreate(const std::string& host, uint16_t port,
                         uint32_t start_tick, uint8_t input_delay) {
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
                      std::string{}, start_tick, input_delay);
    return true;
}

bool Session::RoomJoin(const std::string& host, uint16_t port,
                       const std::string& code,
                       uint32_t start_tick, uint8_t input_delay) {
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
                      code, start_tick, input_delay);
    return true;
}

void Session::RoomSendReady(bool readyFlag) {
    std::vector<uint8_t> pl; pl.push_back(readyFlag ? 1 : 0);
    auto fr = build_frame(MsgType::READY, pl);
    std::lock_guard<std::mutex> lk(roomSendMu_);
    roomSendQ_.push_back(std::move(fr));
}

void Session::RoomLeave() {
    // 가능하면 ROOM_LEAVE 프레임을 먼저 큐에 넣고, quit 를 세워 스레드가
    // 그 프레임을 송신한 뒤 소켓을 닫도록 유도한다.
    auto fr = build_frame(MsgType::ROOM_LEAVE, {});
    {
        std::lock_guard<std::mutex> lk(roomSendMu_);
        roomSendQ_.push_back(std::move(fr));
    }
    quit = true;
    // roomThread 가 즉시 깨어나 큐를 drain 후 닫는다. 소켓은 roomThread 가 닫는다.
}

void Session::roomThread(std::string host, uint16_t port,
                         std::string joinCode,
                         uint32_t start_tick, uint8_t input_delay) {
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

    // 첫 프레임 송신
    std::vector<uint8_t> first;
    if (doCreate) {
        first = build_frame(MsgType::ROOM_CREATE, {});
    } else {
        std::vector<uint8_t> pl;
        pl.push_back(static_cast<uint8_t>(joinCode.size()));
        for (char c : joinCode) pl.push_back(static_cast<uint8_t>(c));
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
        // 아웃바운드 drain (READY / ROOM_LEAVE)
        while (true) {
            std::vector<uint8_t> pkt;
            {
                std::lock_guard<std::mutex> lk(roomSendMu_);
                if (roomSendQ_.empty()) break;
                pkt = std::move(roomSendQ_.front());
                roomSendQ_.pop_front();
            }
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
                recvBuf.clear();
                lastPongMs.store(now_ms());
                lastPingSentMs.store(0);
                roomState_.store(RoomState::Starting);
                ready = true;
                th = std::thread(&Session::ioThread, this);
                return;
            }
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
                          uint32_t start_tick, uint8_t input_delay) {
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

    auto join = build_frame(MsgType::QUEUE_JOIN, {});
    if (!tcp_send_all(sock, join.data(), join.size())) {
        std::cout << "[QUEUE] Failed to send QUEUE_JOIN" << std::endl;
        connectionFailed = true; quit = true;
        return;
    }

    // MATCH_FOUND 대기 — 최대 5분, 2ms 폴링.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    std::vector<uint8_t> buf;
    while (!quit.load()) {
        if (std::chrono::steady_clock::now() >= deadline) {
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
                          << " seed=0x" << std::hex << seed << std::dec << std::endl;
                // ioThread 로 핸드오프 — 이미 수신된 recvBuf 잔여 바이트는 비움.
                recvBuf.clear();
                lastPongMs.store(now_ms());
                lastPingSentMs.store(0);
                ready = true;
                th = std::thread(&Session::ioThread, this);
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // quit 로 나온 경우 (QueueCancel)
    std::cout << "[QUEUE] Cancelled" << std::endl;
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
        }

        size_t prevSize = recvBuf.size();
        if (tcp_recv_some(sock, recvBuf)) {
            if (recvBuf.size() > prevSize) {
                hasActivity = true;
                // 주의: 이 루프는 60Hz+ 로 돈다. 여기서 std::cout 으로 매 recv/parse
                // 를 찍으면 Windows 콘솔 I/O 가 blocking 해 Host 쪽 프레임이 밀린다.
                // 로그가 필요하면 NET_TRACE 매크로 등으로 gate 해 debug 빌드에서만.
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
    std::cout << "[NET] Cleared input queues for restart" << std::endl;
}

}
