#include "player_conn.h"

#include "matchmaker.h"
#include "../net/framing.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

// QUEUE_JOIN 대기 제한 시간. 넘으면 연결 강제 종료.
// 짧게 잡을수록 슬롯/fd 누수 위험 감소, 길게 잡을수록 네트워크 불안정에 관대.
static constexpr auto kJoinTimeout  = std::chrono::seconds(10);
static constexpr auto kPollInterval = std::chrono::milliseconds(10);

void playerConnThread(net::TcpSocket sock, uint32_t conn_id, Matchmaker& mm) {
    std::vector<uint8_t> stream;
    stream.reserve(64);

    const auto deadline = std::chrono::steady_clock::now() + kJoinTimeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (!net::tcp_recv_some(sock, stream)) {
            // EOF/error — 피어가 끊겼거나 소켓 오류
            std::cerr << "[conn " << conn_id << "] disconnected before QUEUE_JOIN\n";
            net::tcp_close(sock);
            return;
        }

        // 새 데이터가 들어왔으면 프레임 파싱 시도
        if (!stream.empty()) {
            std::vector<net::Frame> frames;
            net::parse_frames(stream, frames);
            for (const auto& f : frames) {
                if (f.type == net::MsgType::QUEUE_JOIN) {
                    std::cerr << "[conn " << conn_id << "] QUEUE_JOIN -> queued\n";
                    // 큐에 소켓 소유권 이전 (TcpSocket 은 trivially copyable
                    // 이지만 convention 상 move 로 "이제부터 matchmaker 가 소유"
                    // 를 코드에 드러냄).
                    mm.enqueue({std::move(sock), conn_id});
                    return;
                }
                // 다른 타입(HELLO 등 잘못 보낸 경우)은 무시하고 계속 대기
            }
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    std::cerr << "[conn " << conn_id << "] QUEUE_JOIN timeout -> close\n";
    net::tcp_close(sock);
}

}  // namespace relay
