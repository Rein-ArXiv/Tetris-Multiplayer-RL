// Relay process entry point. Protocol details live in net/framing.h and docs.

#include "matchmaker.h"
#include "player_conn.h"
#include "relay.h"
#include "room.h"
#include "worker_group.h"
#include "../net/socket.h"
#include "../meta/http_client.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>

namespace {

std::atomic<bool> g_running{true};
net::TcpSocket    g_listen_sock{};  // 논블로킹 listen 소켓 (accept 폴링)

// Bound thread and handle use during connection setup.
constexpr size_t kMaxConnWorkers = 256;
constexpr size_t kMaxHandshakesPerIp = 16;

class IpAdmission {
public:
    bool acquire(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        size_t& n = active_[ip.empty() ? "unknown" : ip];
        if (n >= kMaxHandshakesPerIp) return false;
        ++n;
        return true;
    }
    void release(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = active_.find(ip.empty() ? "unknown" : ip);
        if (it == active_.end()) return;
        if (--it->second == 0) active_.erase(it);
    }
private:
    std::mutex mu_;
    std::unordered_map<std::string, size_t> active_;
};

void signalHandler(int /*sig*/) {
    // The signal handler only touches an atomic flag.
    g_running.store(false);
}

void printUsage() {
    std::cout <<
        "Usage: tetris_relay [--port N] [--meta URL] [--meta-secret SECRET]\n"
        "  --port N         TCP listen port (default 7777)\n"
        "  --meta URL       tetris_meta base URL (e.g. https://api.example.com)\n"
        "                   If omitted, relay runs unranked (no token verify,\n"
        "                   no /v1/matches POST).\n"
        "  --meta-secret S  Send X-Relay-Secret on /v1/matches.\n"
        "                   Defaults to TETRIS_RELAY_SECRET if set.\n"
        "  -h, --help       Show this help\n";
}

bool parsePort(const std::string& s, uint16_t& out) {
    if (s.empty()) return false;
    unsigned int value = 0;
    auto* first = s.data();
    auto* last = s.data() + s.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last) return false;
    if (value < 1 || value > 65535) return false;
    out = static_cast<uint16_t>(value);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    uint16_t    port = 7777;
    std::string metaUrl;  // empty = unranked
    std::string metaSecret;
    if (const char* env = std::getenv("TETRIS_RELAY_SECRET")) {
        metaSecret = env;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            const std::string portArg = argv[++i];
            if (!parsePort(portArg, port)) {
                std::cerr << "Invalid --port value: " << portArg << " (expected 1..65535)\n";
                return 2;
            }
        } else if (a == "--meta" && i + 1 < argc) {
            metaUrl = argv[++i];
        } else if (a == "--meta-secret" && i + 1 < argc) {
            metaSecret = argv[++i];
        } else if (a == "-h" || a == "--help") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            printUsage();
            return 1;
        }
    }

    // meta 클라이언트 (옵션). URL 미지정 시 nullptr → unranked.
    std::unique_ptr<meta::client::MetaClient> metaClient;
    if (!metaUrl.empty()) {
        if (metaSecret.empty()) {
            std::cerr << "[relay] refusing to start: --meta set but no relay secret. "
                      << "Set --meta-secret or TETRIS_RELAY_SECRET (meta rejects "
                      << "POST /v1/matches without it).\n";
            return 2;
        }
        metaClient = std::make_unique<meta::client::MetaClient>(metaUrl, metaSecret);
        if (!metaClient->valid()) {
            std::cerr << "[relay] invalid --meta URL: " << metaUrl << "\n";
            return 2;
        } else {
            std::cout << "[relay] meta enabled: " << metaUrl << "\n";
        }
    } else {
        std::cout << "[relay] meta=none (unranked mode)\n";
    }

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    if (!net::net_init()) {
        std::cerr << "net_init() failed\n";
        return 1;
    }

    g_listen_sock = net::tcp_listen(port, /*backlog=*/256);
    if (!g_listen_sock.valid()) {
        std::cerr << "tcp_listen(" << port << ") failed — port in use?\n";
        net::net_shutdown();
        return 1;
    }
    // Nonblocking accept lets the loop observe the shutdown flag.
    net::tcp_set_nonblocking(g_listen_sock);
    std::cout << "[relay] listening on 0.0.0.0:" << port << "\n";
    std::cout << "[relay] local IP: " << net::get_local_ip() << "\n";
    std::cout << "[relay] Ctrl+C to stop\n";

    relay::Matchmaker   mm;
    relay::RoomRegistry rr;

    // Drain workers before destroying the state they reference.
    relay::WorkerGroup connWorkers{"relay-connection", kMaxConnWorkers};
    IpAdmission ipAdmission;

    // 매칭 전담 스레드: 2명 모일 때마다 페어링 + relay 시작.
    // meta 가 있으면 post_match 를 호출할 수 있도록 포인터를 startPump 에 넘긴다.
    meta::client::MetaClient* mcPtr = metaClient.get();
    rr.setMeta(mcPtr);
    std::thread matcher;
    try {
        matcher = std::thread([&mm, mcPtr] {
            try {
                while (true) {
                    auto match = mm.waitForPair();
                    if (!match) break;  // shutdown
                    // 랜덤 큐 경로: MATCH_FOUND → 양쪽 READY(1) 수락 대기 → 게임 포워딩.
                    // (커스텀 룸 경로는 room.cpp 가 READY 를 자체 확인한 뒤 startPump 를 호출한다.)
                    relay::startQueuePump(std::move(*match), mcPtr);
                }
            } catch (const std::exception& e) {
                std::cerr << "[relay] matcher failed: " << e.what() << "\n";
                g_running.store(false);
                mm.shutdown();
            } catch (...) {
                std::cerr << "[relay] matcher failed: unknown exception\n";
                g_running.store(false);
                mm.shutdown();
            }
        });
    } catch (const std::exception& e) {
        std::cerr << "[relay] matcher launch failed: " << e.what() << "\n";
        net::tcp_close(g_listen_sock);
        g_listen_sock = net::TcpSocket{};
        net::net_shutdown();
        return 1;
    }

    // accept 루프 (논블로킹 폴링)
    uint32_t next_conn_id = 1;
    while (g_running.load()) {
        auto client = net::tcp_accept(g_listen_sock);
        if (!client.valid()) {
            // 논블로킹 accept: 대기 연결 없음(EWOULDBLOCK) 또는 셧다운.
            if (!g_running.load()) break;
            // 대기 연결 없음 — 잠깐 쉬었다가 재폴링.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        const uint32_t id = next_conn_id++;
        const std::string peerIp = net::tcp_peer_ip(client);
        if (!ipAdmission.acquire(peerIp)) {
            std::cerr << "[relay] rejecting conn=" << id << " ip=" << peerIp
                      << ": per-IP handshake limit\n";
            net::tcp_close(client);
            continue;
        }
        std::cout << "[relay] accept conn=" << id << "\n";
        if (!connWorkers.launch([client = std::move(client), id, &mm, &rr, mcPtr,
                                 &ipAdmission, peerIp]() mutable {
            struct Release {
                IpAdmission& owner; const std::string& ip;
                ~Release() { owner.release(ip); }
            } release{ipAdmission, peerIp};
            relay::playerConnThread(std::move(client), id, mm, rr, mcPtr);
        })) {
            ipAdmission.release(peerIp);
            std::cerr << "[relay] rejecting conn=" << id
                      << ": connection worker unavailable\n";
        }
    }

    std::cout << "[relay] shutting down...\n";
    relay::beginShutdown();
    connWorkers.stopAccepting();
    net::tcp_close(g_listen_sock);
    g_listen_sock = net::TcpSocket{};  // 마지막 참조 해제 → 실제 fd close
    mm.shutdown();
    rr.shutdown();
    if (matcher.joinable()) matcher.join();
    connWorkers.wait();
    relay::waitForShutdown();
    net::net_shutdown();
    std::cout << "[relay] done\n";
    return 0;
}
