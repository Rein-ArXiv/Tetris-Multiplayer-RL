#include "session.h"
#include <random>
#include <chrono>
#include <iostream>
#include <thread>

namespace net {

// [NET] 세션 수명: 생성/Close에서 스레드와 소켓을 정리합니다.
Session::Session() {}
Session::~Session() { Close(); }

// [NET] 호스트: 연결 수락 후 HELLO/SEED를 송신하여 파라미터를 알립니다.
bool Session::Host(uint16_t port, const SeedParams& sp) {
    // 비동기 수락: UI가 멈추지 않도록 별도 스레드에서 accept
    if (listening) return false;
    seedParams = sp;
    listening = true;
    ath = std::thread(&Session::acceptThread, this, port);
    return true;
}

// [NET] 피어: 원격에 접속 후 HELLO를 보내고 SEED를 기다립니다.
bool Session::Connect(const std::string& host, uint16_t port) {
    std::cout << "[NET] Connecting to " << host << ":" << port << std::endl;
    sock = tcp_connect(host, port);
    if (!sock.valid()) {
        std::cout << "[NET] Failed to connect to " << host << ":" << port << std::endl;
        return false;
    }
    std::cout << "[NET] Connected to " << host << ":" << port << std::endl;
    connected = true;

    // I/O 스레드 시작
    th = std::thread(&Session::ioThread, this);

    // Peer: HELLO 전송, SEED 수신 대기
    {
        std::vector<uint8_t> pl; le_write_u16(pl, 1);
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Sent HELLO message" << std::endl;
    }
    return true;
}

// [NET] 한 틱의 입력을 전송합니다. 지금은 단일 틱만 담지만, 묶어서 보낼 수도 있습니다.
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

// [NET] 원격 입력 조회: 해당 틱의 입력이 도착했는지 확인
bool Session::GetRemoteInput(uint32_t tick, uint8_t& outMask) {
    std::lock_guard<std::mutex> lk(inMu);
    auto it = remoteInputs.find(tick);
    if (it == remoteInputs.end()) return false;
    outMask = it->second; return true;
}

// [NET] 스레드 조인 및 소켓 닫기
void Session::Close() {
    quit = true;
    // 중단: 수락 대기 소켓 닫기
    if (listening && listenSock.valid()) tcp_close(listenSock);
    if (ath.joinable()) ath.join();
    if (th.joinable()) th.join();
    if (sock.valid()) tcp_close(sock);
    connected = false; ready = false; listening = false;
}

// [NET] I/O 스레드: 수신 → 프레이밍 파싱 → 핸들링, 송신 큐 비우기
void Session::ioThread() {
    std::cout << "[NET] I/O thread started" << std::endl;
    auto startTime = std::chrono::steady_clock::now();
    const auto CONNECTION_TIMEOUT = std::chrono::seconds(10); // 10초 타임아웃

    while (!quit.load()) {
        bool hasActivity = false;

        // 연결 타임아웃 체크
        if (!ready.load()) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > CONNECTION_TIMEOUT) {
                std::cout << "[NET] Connection timeout after 10 seconds" << std::endl;
                connectionFailed = true;
                quit = true;
                break;
            }
        }

        // 수신 (논블로킹으로 체크)
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

        // 송신
        {
            std::lock_guard<std::mutex> lk(sendMu);
            while (!sendQ.empty()) {
                hasActivity = true;
                auto& pkt = sendQ.front();
                std::cout << "[NET] Sending packet of size " << pkt.size() << std::endl;
                if (!tcp_send_all(sock, pkt.data(), pkt.size())) {
                    std::cout << "[NET] Send failed!" << std::endl;
                    quit = true;
                    break;
                }
                sendQ.pop_front();
            }
        }

        // CPU 사용률 절약을 위한 짧은 대기
        if (!hasActivity) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

    // I/O 스레드 먼저 시작
    th = std::thread(&Session::ioThread, this);

    // 연결 직후 HELLO/SEED 송신
    {
        std::vector<uint8_t> pl; le_write_u16(pl, 1); // proto ver 1
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Queued HELLO message" << std::endl;
    }
    {
        std::vector<uint8_t> pl;
        le_write_u64(pl, seedParams.seed);
        le_write_u32(pl, seedParams.start_tick);
        pl.push_back(seedParams.input_delay);
        pl.push_back((uint8_t)seedParams.role);
        auto fr = build_frame(MsgType::SEED, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Queued SEED message (seed=0x" << std::hex << seedParams.seed << std::dec << ")" << std::endl;
    }
    // [FIX] 호스트는 SEED를 보낸 후 ready 상태가 됩니다
    ready = true;
    std::cout << "[NET] Host session is ready!" << std::endl;
}

// [NET] 수신 프레임 해석: HELLO/SEED/INPUT/ACK/PING/PONG
void Session::handleFrame(const Frame& f) {
    switch (f.type) {
    case MsgType::HELLO: {
        std::cout << "[NET] Received HELLO message" << std::endl;
        // HELLO 수신 시 간단 ACK 응답
        std::vector<uint8_t> pl; pl.push_back(1); // ok
        auto fr = build_frame(MsgType::HELLO_ACK, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        std::cout << "[NET] Queued HELLO_ACK response" << std::endl;
    } break;
    case MsgType::HELLO_ACK: {
        std::cout << "[NET] Received HELLO_ACK message" << std::endl;
    } break;
    case MsgType::SEED: {
        std::cout << "[NET] Received SEED message" << std::endl;
        // 시드/시작틱/입력지연/역할 적용(합의 완료)
        if (f.payload.size() >= 8+4+1+1) {
            const uint8_t* p = f.payload.data();
            seedParams.seed = le_read_u64(p);
            seedParams.start_tick = le_read_u32(p+8);
            seedParams.input_delay = p[12];
            seedParams.role = (Role)p[13];
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
        if (f.payload.size() >= 4+2) {
            const uint8_t* p = f.payload.data();
            uint32_t from = le_read_u32(p);
            uint16_t cnt = le_read_u16(p+4);
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
            // 간단 ACK(학습용): up_to_tick를 돌려보냄
            std::vector<uint8_t> ack; le_write_u32(ack, lastRemoteTick.load());
            auto fr = build_frame(MsgType::ACK, ack);
            std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
        }
    } break;
    case MsgType::ACK: {
        // 학습용: 미사용
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

}
