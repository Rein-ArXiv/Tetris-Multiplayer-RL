// server/main.cpp — Tetris Multiplayer 릴레이 서버
//
// 빠른 요약:
//   1) TCP 포트(기본 7777) listen
//   2) accept 될 때마다 playerConnThread 스폰 → 해당 스레드가
//      QUEUE_JOIN 프레임을 기다렸다가 matchmaker 큐에 등록
//   3) matcher 스레드가 2명이 모이면 꺼내 relay::startPump() 호출 →
//      양쪽에 MATCH_FOUND 전송 + 바이트 포워딩 시작
//
// 프로토콜(net/framing.h):
//   C→S QUEUE_JOIN   (10) : empty payload
//   S→C MATCH_FOUND  (12) : [role:1][seed:8 LE]   role: 1=HOST, 2=GUEST
//   (이후 바이트는 투명 포워딩 — SEED/INPUT/HASH/GAME_OVER_CHOICE 그대로 통과)

#include "matchmaker.h"
#include "player_conn.h"
#include "relay.h"
#include "room.h"
#include "../net/socket.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_running{true};
net::TcpSocket    g_listen_sock{};  // SIGINT 시 accept() 를 깨우기 위함

void signalHandler(int /*sig*/) {
    g_running.store(false);
    // accept() 는 블로킹이므로 소켓을 닫아 system call 을 반환시킨다.
    // close() 는 signal-safe 하지 않은 구현도 있지만(Windows closesocket 포함)
    // 실전에서는 충분히 동작. 엄격한 async-signal-safety 가 필요하면
    // self-pipe trick 으로 대체 가능.
    net::tcp_close(g_listen_sock);
}

void printUsage() {
    std::cout <<
        "Usage: tetris_relay [--port N]\n"
        "  --port N         TCP listen port (default 7777)\n"
        "  -h, --help       Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    uint16_t port = 7777;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (a == "-h" || a == "--help") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            printUsage();
            return 1;
        }
    }

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    if (!net::net_init()) {
        std::cerr << "net_init() failed\n";
        return 1;
    }

    g_listen_sock = net::tcp_listen(port, /*backlog=*/16);
    if (!g_listen_sock.valid()) {
        std::cerr << "tcp_listen(" << port << ") failed — port in use?\n";
        net::net_shutdown();
        return 1;
    }
    std::cout << "[relay] listening on 0.0.0.0:" << port << "\n";
    std::cout << "[relay] local IP: " << net::get_local_ip() << "\n";
    std::cout << "[relay] Ctrl+C to stop\n";

    relay::Matchmaker   mm;
    relay::RoomRegistry rr;

    // 매칭 전담 스레드: 2명 모일 때마다 페어링 + relay 시작
    std::thread matcher([&mm] {
        while (true) {
            auto match = mm.waitForPair();
            if (!match) break;  // shutdown
            relay::startPump(std::move(*match));
        }
    });

    // accept 루프 (블로킹)
    uint32_t next_conn_id = 1;
    while (g_running.load()) {
        auto client = net::tcp_accept(g_listen_sock);
        if (!client.valid()) {
            // 셧다운 중이면 g_listen_sock 이 이미 닫혔을 것
            if (!g_running.load()) break;
            // 일시적 실패 — 잠깐 쉬었다가 재시도
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        const uint32_t id = next_conn_id++;
        std::cout << "[relay] accept conn=" << id << "\n";
        std::thread(relay::playerConnThread,
                    std::move(client), id, std::ref(mm), std::ref(rr)).detach();
    }

    std::cout << "[relay] shutting down...\n";
    mm.shutdown();
    rr.shutdown();
    if (matcher.joinable()) matcher.join();
    net::net_shutdown();
    std::cout << "[relay] done\n";
    return 0;
}
