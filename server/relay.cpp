#include "relay.h"

#include "../net/framing.h"
#include "../net/socket.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace relay {

namespace {

// 양 방향 스레드가 공유하는 채널 상태.
// refcount 가 0 이 되는 순간(= 양 스레드 모두 종료) 소켓을 닫는다.
struct Channel {
    net::TcpSocket   A;           // HOST 소켓
    net::TcpSocket   B;           // GUEST 소켓
    uint32_t         match_id{0};
    std::atomic<bool> closed{false};
    std::atomic<int>  forwarder_count{2};
};

// 한 방향 포워딩 루프.
//   a_to_b == true  → A 에서 읽어 B 로 쓰기
//   a_to_b == false → B 에서 읽어 A 로 쓰기
void forwarderLoop(std::shared_ptr<Channel> ch, bool a_to_b) {
    const net::TcpSocket& from = a_to_b ? ch->A : ch->B;
    const net::TcpSocket& to   = a_to_b ? ch->B : ch->A;
    const char*           dir  = a_to_b ? "A->B" : "B->A";

    std::vector<uint8_t> buf;
    buf.reserve(4096);

    while (!ch->closed.load()) {
        buf.clear();
        if (!net::tcp_recv_some(from, buf)) {
            // EOF 또는 소켓 오류 — 연결 종료로 간주
            break;
        }
        if (buf.empty()) {
            // 논블로킹: 데이터 없음 → CPU 양보 후 재시도
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (!net::tcp_send_all(to, buf.data(), buf.size())) {
            break;
        }
    }

    std::cerr << "[relay] match=" << ch->match_id << " " << dir << " end\n";
    ch->closed.store(true);

    // 마지막 포워더가 소켓을 닫는다. 먼저 끝난 쪽은 상대방의 read 를
    // EOF 로 깨우지 않음 → closed 플래그를 보고 루프를 빠져나오도록 둔다.
    // (한쪽이 read 에서 블로킹 중이더라도 논블로킹이라 곧 polling 루프로 돌아옴)
    if (--ch->forwarder_count == 0) {
        net::tcp_close(ch->A);
        net::tcp_close(ch->B);
        std::cerr << "[relay] match=" << ch->match_id << " closed\n";
    }
}

// MATCH_FOUND 프레임 전송. 페이로드: [role:1][seed:8 LE]
bool sendMatchFound(const net::TcpSocket& sock, uint8_t role, uint64_t seed) {
    std::vector<uint8_t> payload;
    payload.reserve(9);
    payload.push_back(role);
    net::le_write_u64(payload, seed);
    auto frame = net::build_frame(net::MsgType::MATCH_FOUND, payload);
    return net::tcp_send_all(sock, frame.data(), frame.size());
}

}  // namespace

void startPump(Match match) {
    // 역할 배정: a = HOST (role=1), b = GUEST (role=2)
    //   MsgType::MATCH_FOUND 주석의 role 값과 일치해야 함 (net/framing.h).
    constexpr uint8_t ROLE_HOST  = 1;
    constexpr uint8_t ROLE_GUEST = 2;

    const bool ok_a = sendMatchFound(match.a.sock, ROLE_HOST,  match.seed);
    const bool ok_b = sendMatchFound(match.b.sock, ROLE_GUEST, match.seed);

    if (!ok_a || !ok_b) {
        std::cerr << "[relay] MATCH_FOUND send failed, match=" << match.match_id << "\n";
        net::tcp_close(match.a.sock);
        net::tcp_close(match.b.sock);
        return;
    }

    std::cerr << "[relay] match start id=" << match.match_id
              << " HOST=conn" << match.a.conn_id
              << " GUEST=conn" << match.b.conn_id
              << " seed=0x" << std::hex << match.seed << std::dec << "\n";

    // 양 방향 포워더 기동. shared_ptr 로 수명 공유.
    auto ch = std::make_shared<Channel>();
    ch->A        = match.a.sock;
    ch->B        = match.b.sock;
    ch->match_id = match.match_id;

    std::thread(forwarderLoop, ch, true ).detach();
    std::thread(forwarderLoop, ch, false).detach();
}

}  // namespace relay
