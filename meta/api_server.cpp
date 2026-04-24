#include "api_server.h"
#include "protocol.h"

// cpp-httplib 는 windows.h 와 상호작용이 있어서 WIN32_LEAN_AND_MEAN 정의 후 include.
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
#endif
#include "httplib.h"

#include <cstdio>
#include <cstdint>
#include <random>
#include <string>

namespace meta {

namespace {

// 32 hex chars 무작위 토큰 (16 바이트 엔트로피). std::random_device 는 플랫폼별
// OS CSPRNG (Windows: CryptGenRandom, Linux: /dev/urandom) 을 래핑. 암호학적으로
// 충분히 강함.
std::string gen_token()
{
    std::random_device rd;
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    char buf[33];
    for (int i = 0; i < 4; ++i) {
        uint32_t x = dist(rd);
        std::snprintf(buf + i * 8, 9, "%08x", x);
    }
    buf[32] = '\0';
    return std::string(buf, 32);
}

// CORS + content-type 을 한 번에 세팅.
void set_json(httplib::Response& res, int status, const std::string& body)
{
    res.status = status;
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_content(body, "application/json");
}

} // namespace

ApiServer::ApiServer(Database& db) : db_(db) {}

bool ApiServer::listen(const std::string& host, int port)
{
    httplib::Server svr;

    // ------- CORS preflight (브라우저 정적 페이지용) ------------------------
    svr.Options(R"(/v1/.*)",
        [](const httplib::Request&, httplib::Response& res) {
            res.status = 204;
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
        });

    // ------- POST /v1/guest -------------------------------------------------
    svr.Post("/v1/guest",
        [this](const httplib::Request&, httplib::Response& res) {
            // 토큰 충돌은 16 바이트 엔트로피에서 사실상 불가능하지만,
            // registerGuest 가 nullopt 반환 시 한 번만 재시도.
            for (int attempt = 0; attempt < 2; ++attempt) {
                auto token = gen_token();
                auto p = db_.registerGuest(token);
                if (p) {
                    set_json(res, 200, proto::guest_response(p->id, p->token, p->elo));
                    std::fprintf(stderr, "[meta] guest player_id=%lld\n",
                                 static_cast<long long>(p->id));
                    return;
                }
            }
            set_json(res, 500, proto::error_json("register_failed", "db write failed"));
        });

    // ------- POST /v1/auth/verify ------------------------------------------
    svr.Post("/v1/auth/verify",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string token = proto::find_string(req.body, "token");
            if (token.empty()) {
                set_json(res, 400, proto::error_json("bad_request", "missing token"));
                return;
            }
            auto p = db_.getByToken(token);
            if (!p) {
                set_json(res, 404, proto::error_json("unknown_token"));
                return;
            }
            set_json(res, 200,
                proto::auth_response(p->id, p->username, p->elo));
        });

    // ------- POST /v1/matches ----------------------------------------------
    svr.Post("/v1/matches",
        [this](const httplib::Request& req, httplib::Response& res) {
            auto pa = proto::find_int(req.body, "player_a");
            auto pb = proto::find_int(req.body, "player_b");
            auto wn = proto::find_int(req.body, "winner");   // null 허용
            auto sa = proto::find_int(req.body, "score_a");
            auto sb = proto::find_int(req.body, "score_b");
            auto la = proto::find_int(req.body, "lines_a");
            auto lb = proto::find_int(req.body, "lines_b");
            auto du = proto::find_int(req.body, "duration_s");

            if (!pa || !pb || !sa || !sb || !la || !lb || !du) {
                set_json(res, 400,
                    proto::error_json("bad_request", "missing required fields"));
                return;
            }
            if (*pa == *pb) {
                set_json(res, 400,
                    proto::error_json("bad_request", "player_a == player_b"));
                return;
            }
            // winner 가 있다면 player_a 또는 player_b 중 하나여야 한다.
            // 그렇지 않으면 ELO 갱신이 두 플레이어 모두 losses 만 누적하는 잘못된
            // 상태를 만든다 (saveMatch 가 winner != a && winner != b 인 분기에서
            // 둘 다 패배 처리). 외부에 노출되는 API 이므로 여기서 막는다.
            if (wn && (*wn != *pa && *wn != *pb)) {
                set_json(res, 400,
                    proto::error_json("bad_request", "winner must be player_a, player_b, or null"));
                return;
            }
            if (*sa < 0 || *sb < 0 || *la < 0 || *lb < 0 || *du < 0) {
                set_json(res, 400,
                    proto::error_json("bad_request", "scores/lines/duration must be non-negative"));
                return;
            }

            MatchRecord m;
            m.player_a   = *pa;
            m.player_b   = *pb;
            m.winner     = wn;  // optional passthrough
            m.score_a    = static_cast<int>(*sa);
            m.score_b    = static_cast<int>(*sb);
            m.lines_a    = static_cast<int>(*la);
            m.lines_b    = static_cast<int>(*lb);
            m.duration_s = static_cast<int>(*du);

            auto ins = db_.saveMatch(m);
            if (!ins) {
                set_json(res, 500,
                    proto::error_json("save_failed", "db transaction failed"));
                return;
            }

            const proto::SideDelta a{ ins->a.elo_before, ins->a.elo_after, ins->a.delta };
            const proto::SideDelta b{ ins->b.elo_before, ins->b.elo_after, ins->b.delta };
            set_json(res, 200, proto::matches_response(ins->match_id, a, b));
            std::fprintf(stderr, "[meta] match=%lld a=%+d b=%+d\n",
                         static_cast<long long>(ins->match_id),
                         ins->a.delta, ins->b.delta);
        });

    // ------- GET /v1/leaderboard -------------------------------------------
    svr.Get("/v1/leaderboard",
        [this](const httplib::Request& req, httplib::Response& res) {
            int limit = 20;
            if (req.has_param("limit")) {
                try {
                    limit = std::stoi(req.get_param_value("limit"));
                } catch (...) { limit = 20; }
            }
            auto rows = db_.leaderboard(limit);

            std::vector<proto::LeaderRow> out;
            out.reserve(rows.size());
            for (const auto& r : rows) {
                out.push_back({ r.player_id, r.username, r.elo, r.wins, r.losses });
            }
            set_json(res, 200, proto::leaderboard_response(out));
        });

    std::fprintf(stderr, "[meta] HTTP listening on %s:%d\n", host.c_str(), port);
    bool ok = svr.listen(host, port);
    if (!ok) std::fprintf(stderr, "[meta] listen failed on %s:%d\n", host.c_str(), port);
    return ok;
}

} // namespace meta
