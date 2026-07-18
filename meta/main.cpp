// meta/main.cpp — tetris_meta 진입점.
//
// 사용법:
//   tetris_meta [--db PATH] [--http HOST:PORT] [--relay-secret SECRET]
//               [--allow-public-matches]
//
// 기본값: --db tetris.db  --http 127.0.0.1:8080
//
// 흐름:
//   1. CLI 파싱
//   2. Database(path) 생성 (스키마 자동 적용)
//   3. ApiServer 에 Database 주입 후 blocking listen 호출
//   4. Ctrl+C → httplib 이 내부 시그널 핸들러로 stop 해서 listen() 반환

#include "api_server.h"
#include "database.h"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

struct Args {
    std::string db_path  = "tetris.db";
    std::string http_host = "127.0.0.1";
    int         http_port = 8080;
    std::string relay_secret;
    bool        allow_public_matches = false;
};

// host[:port] 형태 파싱. port 생략 시 기존 값 유지.
bool parse_endpoint(const std::string& s, std::string& host, int& port)
{
    if (s.empty()) return false;
    auto pos = s.rfind(':');
    if (pos == std::string::npos) {
        host = s;
        return !host.empty();
    }
    const std::string parsed_host = s.substr(0, pos);
    const std::string port_s = s.substr(pos + 1);
    if (parsed_host.empty() || port_s.empty()) return false;

    int parsed_port = 0;
    auto* first = port_s.data();
    auto* last = port_s.data() + port_s.size();
    auto res = std::from_chars(first, last, parsed_port);
    if (res.ec != std::errc{} || res.ptr != last) return false;
    port = parsed_port;
    host = parsed_host;
    if (port < 1 || port > 65535) return false;
    return true;
}

void print_usage()
{
    std::fprintf(stderr,
        "tetris_meta — HTTP+SQLite metadata server\n"
        "\n"
        "Usage:\n"
        "  tetris_meta [--db PATH] [--http HOST:PORT] [--relay-secret SECRET]\n"
        "              [--allow-public-matches]\n"
        "\n"
        "Defaults:\n"
        "  --db    tetris.db\n"
        "  --http  127.0.0.1:8080\n"
        "\n"
        "Security:\n"
        "  --relay-secret SECRET  Shared secret required on POST /v1/matches.\n"
        "                         Defaults to TETRIS_RELAY_SECRET if set.\n"
        "  --allow-public-matches Allow unauthenticated POST /v1/matches.\n"
        "                         Intended only for local development/tests.\n"
        "\n"
        "Endpoints:\n"
        "  POST /v1/guest        — create anonymous player, returns token + elo(RP)=0\n"
        "  POST /v1/auth/verify  — {\"token\":\"...\"} → player info (404 if unknown)\n"
        "  POST /v1/matches      — save result + update RP, returns deltas\n"
        "  GET  /v1/leaderboard  — top N (default 20, max 100)\n"
        "  GET  /v1/icons/catalog — avatar icon catalog\n"
        "  POST /v1/icons/buy     — {token,icon_id} BP purchase\n"
        "  POST /v1/icons/select  — {token,icon_id} select owned icon\n"
        "  GET  /healthz          — process health check\n");
}

Args parse_args(int argc, char** argv)
{
    Args a;
    if (const char* env = std::getenv("TETRIS_RELAY_SECRET")) {
        a.relay_secret = env;
    }
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (k == "--db" && i + 1 < argc) {
            a.db_path = argv[++i];
        } else if (k == "--http" && i + 1 < argc) {
            if (!parse_endpoint(argv[++i], a.http_host, a.http_port)) {
                std::fprintf(stderr, "invalid --http value\n");
                std::exit(2);
            }
        } else if (k == "--relay-secret" && i + 1 < argc) {
            a.relay_secret = argv[++i];
        } else if (k == "--allow-public-matches") {
            a.allow_public_matches = true;
        } else if (k == "-h" || k == "--help") {
            print_usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", k.c_str());
            print_usage();
            std::exit(2);
        }
    }
    return a;
}

} // namespace

int main(int argc, char** argv)
{
    const Args args = parse_args(argc, argv);

    if (args.relay_secret.empty() && !args.allow_public_matches) {
        std::fprintf(stderr,
                     "[meta] refusing to start: POST /v1/matches requires "
                     "--relay-secret or TETRIS_RELAY_SECRET. For local-only "
                     "tests, pass --allow-public-matches explicitly.\n");
        return 2;
    }

    std::fprintf(stderr, "[meta] opening db: %s\n", args.db_path.c_str());

    std::unique_ptr<meta::Database> db;
    try {
        db = std::make_unique<meta::Database>(args.db_path);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[meta] db open failed: %s\n", e.what());
        return 1;
    }
    std::fprintf(stderr, "[meta] schema ready\n");

    if (args.relay_secret.empty()) {
        std::fprintf(stderr,
                     "[meta] warning: /v1/matches accepts public requests "
                     "(--allow-public-matches)\n");
    } else {
        std::fprintf(stderr, "[meta] /v1/matches requires X-Relay-Secret\n");
    }

    meta::ApiServer api(*db, args.relay_secret);
    if (!api.listen(args.http_host, args.http_port)) {
        return 1;
    }
    return 0;
}
