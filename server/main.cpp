// Relay process entry point. Protocol details live in net/framing.h and docs.

#include "ip_admission.h"
#include "matchmaker.h"
#include "player_conn.h"
#include "relay.h"
#include "room.h"
#include "worker_group.h"
#include "../net/socket.h"
#include "../meta/http_client.h"
#include "log.h"

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
#include <string>
#include <system_error>
#include <thread>

namespace {

std::atomic<bool> g_running{true};
net::TcpSocket    g_listen_sock{};  // 논블로킹 listen 소켓 (accept 폴링)

// Bound thread and handle use during connection setup.
constexpr size_t kMaxConnWorkers = 256;

// per-IP 상한은 server/ip_admission.h 가 두 릴레이 바이너리에 공통으로 정의한다
// (핸드셰이크 슬롯 = 인증까지, 세션 슬롯 = 연결이 죽을 때까지).

void signalHandler(int /*sig*/) {
    // The signal handler only touches an atomic flag.
    g_running.store(false);
}

void printUsage() {
    std::cout <<
        "Usage: tetris_relay [--port N] [--meta URL] [--meta-secret SECRET]\n"
        "                    [--max-sessions-per-ip N] [--log-level L]\n"
        "  --log-level L    error|warn|info|debug (default info). 운영에서는 warn 이\n"
        "                   접속·매치 줄까지 지운다. TETRIS_RELAY_LOG_LEVEL 로도\n"
        "                   정할 수 있고 이 인자가 이긴다.\n"
        "  --port N         TCP listen port (default 7777)\n"
        "  --meta URL       tetris_meta base URL (e.g. https://api.example.com)\n"
        "                   If omitted, relay runs unranked (no token verify,\n"
        "                   no /v1/matches POST).\n"
        "  --meta-secret S  Send X-Relay-Secret on /v1/matches.\n"
        "                   Defaults to TETRIS_RELAY_SECRET if set.\n"
        "  --max-sessions-per-ip N\n"
        "                   Concurrent connections one address may hold for the\n"
        "                   life of the connection (default "
        << relay::kMaxSessionsPerIp << ").\n"
        "                   Separate from the per-IP handshake budget ("
        << relay::kMaxHandshakesPerIp << "), which\n"
        "                   is released as soon as a connection authenticates.\n"
        "                   Raise it only for a deployment that legitimately\n"
        "                   shares one address across many players.\n"
        "  -h, --help       Show this help\n";
}

bool parseCount(const std::string& s, size_t& out) {
    if (s.empty()) return false;
    unsigned long long value = 0;
    auto* first = s.data();
    auto* last = s.data() + s.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last) return false;
    if (value < 1 || value > 100000) return false;
    out = static_cast<size_t>(value);
    return true;
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

    // 환경변수 먼저, 인자 나중 — 루프 릴레이와 같은 규칙이다. 두 바이너리가 로그
    // 설정을 다르게 받으면 운영자가 바이너리마다 다른 것을 외워야 한다.
    // 잘못된 값은 알린 뒤 기본값으로 간다. 로그 설정 하나로 서버가 안 뜨는 쪽이
    // 더 나쁜 실패다.
    if (const char* env = std::getenv("TETRIS_RELAY_LOG_LEVEL")) {
        relay::LogLevel lv{};
        if (relay::parse_log_level(env, lv)) relay::set_log_level(lv);
        else RLOG_WARN("[relay] TETRIS_RELAY_LOG_LEVEL 값을 알 수 없어 무시합니다: "
                       << env);
    }

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--log-level" && i + 1 < argc) {
            const std::string v = argv[++i];
            relay::LogLevel lv{};
            if (!relay::parse_log_level(v, lv)) {
                RLOG_ERROR("[relay] --log-level 은 error|warn|info|debug 여야 합니다: " << v);
                return 2;
            }
            relay::set_log_level(lv);
        } else if (a == "--port" && i + 1 < argc) {
            const std::string portArg = argv[++i];
            if (!parsePort(portArg, port)) {
                RLOG_ERROR("Invalid --port value: " << portArg
                           << " (expected 1..65535)");
                return 2;
            }
        } else if (a == "--meta" && i + 1 < argc) {
            metaUrl = argv[++i];
        } else if (a == "--meta-secret" && i + 1 < argc) {
            metaSecret = argv[++i];
        } else if (a == "--max-sessions-per-ip" && i + 1 < argc) {
            const std::string arg = argv[++i];
            size_t n = 0;
            if (!parseCount(arg, n)) {
                RLOG_ERROR("Invalid --max-sessions-per-ip value: " << arg
                           << " (expected 1..100000)");
                return 2;
            }
            relay::IpAdmission::set_session_limit(n);
        } else if (a == "-h" || a == "--help") {
            printUsage();
            return 0;
        } else {
            RLOG_ERROR("Unknown arg: " << a);
            printUsage();
            return 1;
        }
    }

    // meta 클라이언트 (옵션). URL 미지정 시 nullptr → unranked.
    std::unique_ptr<meta::client::MetaClient> metaClient;
    if (!metaUrl.empty()) {
        if (metaSecret.empty()) {
            RLOG_ERROR("[relay] refusing to start: --meta set but no relay secret. "
                       << "Set --meta-secret or TETRIS_RELAY_SECRET (meta rejects "
                       << "POST /v1/matches without it).");
            return 2;
        }
        metaClient = std::make_unique<meta::client::MetaClient>(metaUrl, metaSecret);
        if (!metaClient->valid()) {
            RLOG_ERROR("[relay] invalid --meta URL: " << metaUrl);
            return 2;
        } else {
            RLOG_INFO("[relay] meta enabled: " << metaUrl);
        }
    } else {
        RLOG_INFO("[relay] meta=none (unranked mode)");
    }

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
#if defined(_WIN32)
    // Windows 콘솔의 CTRL_BREAK_EVENT 는 CRT 가 SIGBREAK 로 전달한다. Python
    // 테스트가 TerminateProcess(핸들러 실행 기회가 아예 없다) 대신
    // CTRL_BREAK_EVENT 로 우아한 종료 경로를 검증할 수 있도록 함께 등록한다.
    std::signal(SIGBREAK, signalHandler);
#endif

    if (!net::net_init()) {
        RLOG_ERROR("net_init() failed");
        return 1;
    }

    g_listen_sock = net::tcp_listen(port, /*backlog=*/256);
    if (!g_listen_sock.valid()) {
        RLOG_ERROR("tcp_listen(" << port << ") failed — port in use?");
        net::net_shutdown();
        return 1;
    }
    // Nonblocking accept lets the loop observe the shutdown flag.
    net::tcp_set_nonblocking(g_listen_sock);
    RLOG_INFO("[relay] listening on 0.0.0.0:" << port);
    RLOG_INFO("[relay] local IP: " << net::get_local_ip());
    RLOG_INFO("[relay] Ctrl+C to stop");

    relay::Matchmaker   mm;
    relay::RoomRegistry rr;

    // Drain workers before destroying the state they reference.
    relay::WorkerGroup connWorkers{"relay-connection", kMaxConnWorkers};
    RLOG_INFO("[relay] per-IP limits: handshakes=" << relay::kMaxHandshakesPerIp
              << " sessions=" << relay::IpAdmission::session_limit());

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
                RLOG_ERROR("[relay] matcher failed: " << e.what());
                g_running.store(false);
                mm.shutdown();
            } catch (...) {
                RLOG_ERROR("[relay] matcher failed: unknown exception");
                g_running.store(false);
                mm.shutdown();
            }
        });
    } catch (const std::exception& e) {
        RLOG_ERROR("[relay] matcher launch failed: " << e.what());
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
        std::string peerIp = net::tcp_peer_ip(client);
        if (peerIp.empty()) {
            // getpeername 실패 시 모든 연결이 "unknown" 단일 버킷(상한 16)을
            // 공유하면 무관한 연결끼리 서로를 굶긴다. fd 는 이 연결이 살아있는
            // 동안 프로세스 내에서 유일하므로 연결별 고유 키로 대신 사용한다
            // (per-IP 상한은 못 걸지만, 실패 케이스끼리의 공멸보다 낫다).
            peerIp = "fd:" + std::to_string(client.fd());
        }
        // 두 상한을 독립적으로 건다. 세션 슬롯은 연결이 죽을 때까지(소켓과 함께
        // 큐·룸·포워딩으로 옮겨 다니며) 붙들고, 핸드셰이크 슬롯은 인증이 끝나는
        // 순간 playerConnThread 가 놓아준다.
        auto sessionSlot = relay::IpAdmission::acquire(
            peerIp, relay::IpAdmission::Kind::Session);
        if (!sessionSlot) {
            RLOG_INFO("[relay] rejecting conn=" << id << " ip=" << peerIp
                      << ": per-IP session limit player_id=0 match_uuid=-");
            net::tcp_close(client);
            continue;
        }
        auto handshakeSlot = relay::IpAdmission::acquire(
            peerIp, relay::IpAdmission::Kind::Handshake);
        if (!handshakeSlot) {
            RLOG_INFO("[relay] rejecting conn=" << id << " ip=" << peerIp
                      << ": per-IP handshake limit player_id=0 match_uuid=-");
            net::tcp_close(client);
            continue;
        }
        RLOG_DEBUG("[relay] accept conn=" << id << " ip=" << peerIp);
        // launch 가 실패하면 람다(그리고 두 슬롯 사본)가 그대로 소멸하므로
        // 별도의 반납 경로가 필요 없다.
        if (!connWorkers.launch([client = std::move(client), id, &mm, &rr, mcPtr,
                                 handshakeSlot = std::move(handshakeSlot),
                                 sessionSlot = std::move(sessionSlot)]() mutable {
            relay::playerConnThread(std::move(client), id, mm, rr, mcPtr,
                                    std::move(handshakeSlot),
                                    std::move(sessionSlot));
        })) {
            RLOG_WARN("[relay] rejecting conn=" << id
                      << ": connection worker unavailable player_id=0 match_uuid=-");
        }
    }

    RLOG_INFO("[relay] shutting down...");
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
    RLOG_INFO("[relay] done");
    return 0;
}
