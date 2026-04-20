#include "player_conn.h"

#include "matchmaker.h"
#include "room.h"
#include "../net/framing.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

// 첫 프레임(QUEUE_JOIN / ROOM_CREATE / ROOM_JOIN) 대기 제한 시간.
// 클라이언트는 TCP connect 직후 바로 첫 프레임을 보내므로 10초면 충분.
static constexpr auto kJoinTimeout  = std::chrono::seconds(10);
static constexpr auto kPollInterval = std::chrono::milliseconds(10);

void playerConnThread(net::TcpSocket sock, uint32_t conn_id,
                      Matchmaker& mm, RoomRegistry& rr) {
    std::vector<uint8_t> stream;
    stream.reserve(64);

    const auto deadline = std::chrono::steady_clock::now() + kJoinTimeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (!net::tcp_recv_some(sock, stream)) {
            std::cerr << "[conn " << conn_id << "] disconnected before first frame\n";
            net::tcp_close(sock);
            return;
        }

        if (!stream.empty()) {
            std::vector<net::Frame> frames;
            net::parse_frames(stream, frames);
            for (const auto& f : frames) {
                if (f.type == net::MsgType::QUEUE_JOIN) {
                    std::cerr << "[conn " << conn_id << "] QUEUE_JOIN -> queued\n";
                    mm.enqueue({std::move(sock), conn_id});
                    return;
                }
                if (f.type == net::MsgType::QUEUE_CANCEL) {
                    std::cerr << "[conn " << conn_id << "] QUEUE_CANCEL before queued\n";
                    net::tcp_close(sock);
                    return;
                }
                if (f.type == net::MsgType::ROOM_CREATE) {
                    std::cerr << "[conn " << conn_id << "] ROOM_CREATE\n";
                    rr.handleCreate(std::move(sock), conn_id);
                    return;
                }
                if (f.type == net::MsgType::ROOM_JOIN) {
                    if (f.payload.size() < 1) continue;
                    const uint8_t n = f.payload[0];
                    // 룸 코드는 room.cpp 의 kCodeLen=5 자 고정. 상한을 두어 악성/
                    // 손상 페이로드가 handleJoin 까지 내려가지 않도록 방어.
                    constexpr uint8_t kMaxCodeLen = 5;
                    if (n == 0 || n > kMaxCodeLen ||
                        f.payload.size() < 1u + n) continue;
                    std::string code(f.payload.begin() + 1,
                                     f.payload.begin() + 1 + n);
                    std::cerr << "[conn " << conn_id << "] ROOM_JOIN " << code << "\n";
                    rr.handleJoin(code, std::move(sock), conn_id);
                    return;
                }
                // HELLO 등 낯선 프레임은 초기 phase 에서는 무시 + 계속 대기
            }
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    std::cerr << "[conn " << conn_id << "] first-frame timeout -> close\n";
    net::tcp_close(sock);
}

}  // namespace relay
