#include "session.h"
#include <random>
#include <chrono>

namespace net {

// [NET] 세션 수명: 생성/Close에서 스레드와 소켓을 정리합니다.
Session::Session() {}
Session::~Session() { Close(); }

// [NET] 호스트: 연결 수락 후 HELLO/SEED를 송신하여 파라미터를 알립니다.
bool Session::Host(uint16_t port, const SeedParams& sp) {
    auto server = tcp_listen(port, 1);
    if (!server.valid()) return false;
    auto client = tcp_accept(server);
    tcp_close(server);
    if (!client.valid()) return false;
    sock = client;
    connected = true;
    // 연결 직후 HELLO/SEED 송신
    {
        std::vector<uint8_t> pl; le_write_u16(pl, 1); // proto ver 1
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    }
    seedParams = sp;
    {
        std::vector<uint8_t> pl; 
        le_write_u64(pl, seedParams.seed);
        le_write_u32(pl, seedParams.start_tick);
        pl.push_back(seedParams.input_delay);
        pl.push_back((uint8_t)seedParams.role);
        auto fr = build_frame(MsgType::SEED, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    }
    // Host knows parameters already; mark ready to allow UI to proceed.
    ready = true;
    th = std::thread(&Session::ioThread, this);
    return true;
}

// [NET] 피어: 원격에 접속 후 HELLO를 보내고 SEED를 기다립니다.
bool Session::Connect(const std::string& host, uint16_t port) {
    sock = tcp_connect(host, port);
    if (!sock.valid()) return false;
    connected = true;
    // Peer: HELLO 전송, SEED 수신 대기
    {
        std::vector<uint8_t> pl; le_write_u16(pl, 1);
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    }
    th = std::thread(&Session::ioThread, this);
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
    if (th.joinable()) th.join();
    if (sock.valid()) tcp_close(sock);
    connected = false; ready = false;
}

// [NET] I/O 스레드: 수신 → 프레이밍 파싱 → 핸들링, 송신 큐 비우기
void Session::ioThread() {
    while (!quit.load()) {
        // 수신
        if (!tcp_recv_some(sock, recvBuf)) { quit = true; break; }
        std::vector<Frame> frames; parse_frames(recvBuf, frames);
        for (auto& f : frames) handleFrame(f);
        // 송신
        {
            std::lock_guard<std::mutex> lk(sendMu);
            while (!sendQ.empty()) {
                auto& pkt = sendQ.front();
                if (!tcp_send_all(sock, pkt.data(), pkt.size())) { quit = true; break; }
                sendQ.pop_front();
            }
        }
    }
}

// [NET] 수신 프레임 해석: HELLO/SEED/INPUT/ACK/PING/PONG
void Session::handleFrame(const Frame& f) {
    switch (f.type) {
    case MsgType::HELLO: {
        // HELLO 수신 시 간단 ACK 응답
        std::vector<uint8_t> pl; pl.push_back(1); // ok
        auto fr = build_frame(MsgType::HELLO_ACK, pl);
        std::lock_guard<std::mutex> lk(sendMu); sendQ.push_back(std::move(fr));
    } break;
    case MsgType::HELLO_ACK: {
        // nop
    } break;
    case MsgType::SEED: {
        // 시드/시작틱/입력지연/역할 적용(합의 완료)
        if (f.payload.size() >= 8+4+1+1) {
            const uint8_t* p = f.payload.data();
            seedParams.seed = le_read_u64(p);
            seedParams.start_tick = le_read_u32(p+8);
            seedParams.input_delay = p[12];
            seedParams.role = (Role)p[13];
            ready = true;
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
