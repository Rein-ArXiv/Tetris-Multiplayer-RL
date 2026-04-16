#include "session.h"
#include <random>
#include <chrono>
#include <iostream>
#include <thread>

namespace net {

Session::Session() {}
Session::~Session() { Close(); }

bool Session::Host(uint16_t port, const SeedParams& sp) {
    if (listening) return false;

    // Close() 이후 재사용을 위한 상태 리셋
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

    { std::lock_guard<std::mutex> lk(seedMu); seedParams = sp; }
    listening = true;
    ath = std::thread(&Session::acceptThread, this, port);
    return true;
}

bool Session::Adopt(TcpSocket socket, Role role, uint64_t seed,
                    uint32_t start_tick, uint8_t input_delay) {
    if (!socket.valid()) return false;

    // Close() 이후 재사용을 위한 상태 리셋
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
    ready = true;
    th = std::thread(&Session::ioThread, this);
    std::cout << "[NET] Adopted relay socket: role="
              << (role == Role::Host ? "HOST" : "GUEST")
              << " seed=0x" << std::hex << seed << std::dec << std::endl;
    return true;
}

bool Session::Connect(const std::string& host, uint16_t port) {
    std::cout << "[NET] Connecting to " << host << ":" << port << std::endl;

    // Close() 이후 재사용을 위한 상태 리셋
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
    if (th.joinable()) th.join();
    connected = false; ready = false; listening = false;
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

        size_t prevSize = recvBuf.size();
        if (tcp_recv_some(sock, recvBuf)) {
            if (recvBuf.size() > prevSize) {
                hasActivity = true;
                std::cout << "[NET] Received " << (recvBuf.size() - prevSize) << " bytes" << std::endl;
                std::vector<Frame> frames;
                parse_frames(recvBuf, frames);
                if (!frames.empty()) {
                    std::cout << "[NET] Parsed " << frames.size() << " frames" << std::endl;
                }
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
            lastHashTickRemote.store(t);
            lastHashRemote.store(h);
        }
    } break;
    case MsgType::GAME_OVER_CHOICE: {
        if (f.payload.size() >= 1) {
            uint8_t choice = f.payload[0];
            remoteGameOverChoice.store(choice);
            std::cout << "[NET] Received game over choice: " << (int)choice << std::endl;
        }
    } break;
    case MsgType::PING: {
        std::vector<uint8_t> pong = f.payload; auto fr = build_frame(MsgType::PONG, pong);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    } break;
    case MsgType::PONG: {
        // RTT 측정 지점
    } break;
    default: break;
    }
}

bool Session::GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const {
    tick = lastHashTickRemote.load();
    hash = lastHashRemote.load();
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
    std::lock_guard<std::mutex> lk(inMu);
    remoteInputs.clear();
    lastRemoteTick.store(0);
    lastLocalTick.store(0);
    std::cout << "[NET] Cleared input queues for restart" << std::endl;
}

}
