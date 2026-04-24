// meta/main.cpp — tetris_meta 진입점.
//
// 사용법:
//   tetris_meta [--db PATH] [--http HOST:PORT]
//
// 기본값: --db tetris.db  --http 0.0.0.0:8080
//
// 흐름:
//   1. CLI 파싱
//   2. Database(path) 생성 (스키마 자동 적용)
//   3. ApiServer 에 Database 주입 후 blocking listen 호출
//   4. Ctrl+C → httplib 이 내부 시그널 핸들러로 stop 해서 listen() 반환

#include "api_server.h"
#include "database.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

struct Args {
    std::string db_path  = "tetris.db";
    std::string http_host = "0.0.0.0";
    int         http_port = 8080;
};

// host[:port] 형태 파싱. port 생략 시 기존 값 유지.
bool parse_endpoint(const std::string& s, std::string& host, int& port)
{
    if (s.empty()) return false;
    auto pos = s.rfind(':');
    if (pos == std::string::npos) { host = s; return true; }
    host = s.substr(0, pos);
    try {
        port = std::stoi(s.substr(pos + 1));
    } catch (...) {
        return false;
    }
    if (port < 1 || port > 65535) return false;
    return true;
}

void print_usage()
{
    std::fprintf(stderr,
        "tetris_meta — HTTP+SQLite metadata server\n"
        "\n"
        "Usage:\n"
        "  tetris_meta [--db PATH] [--http HOST:PORT]\n"
        "\n"
        "Defaults:\n"
        "  --db    tetris.db\n"
        "  --http  0.0.0.0:8080\n"
        "\n"
        "Endpoints:\n"
        "  POST /v1/guest        — create anonymous player, returns token + elo=1200\n"
        "  POST /v1/auth/verify  — {\"token\":\"...\"} → player info (404 if unknown)\n"
        "  POST /v1/matches      — save result + update ELO, returns deltas\n"
        "  GET  /v1/leaderboard  — top N (default 20, max 100)\n");
}

Args parse_args(int argc, char** argv)
{
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (k == "--db" && i + 1 < argc) {
            a.db_path = argv[++i];
        } else if (k == "--http" && i + 1 < argc) {
            if (!parse_endpoint(argv[++i], a.http_host, a.http_port)) {
                std::fprintf(stderr, "invalid --http value\n");
                std::exit(2);
            }
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

    std::fprintf(stderr, "[meta] opening db: %s\n", args.db_path.c_str());

    std::unique_ptr<meta::Database> db;
    try {
        db = std::make_unique<meta::Database>(args.db_path);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[meta] db open failed: %s\n", e.what());
        return 1;
    }
    std::fprintf(stderr, "[meta] schema ready\n");

    meta::ApiServer api(*db);
    if (!api.listen(args.http_host, args.http_port)) {
        return 1;
    }
    return 0;
}
