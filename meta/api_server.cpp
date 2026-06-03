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

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace meta {

namespace {

// [보안] OS CSPRNG 에서 n 바이트의 무작위 데이터를 채운다.
//   std::random_device 는 대부분의 타깃에서 OS CSPRNG 를 래핑하지만 일부
//   구현(예: 구형 MinGW)에서는 결정적일 수 있다. 토큰/세션 비밀에는 OS
//   엔트로피를 명시적으로 읽고, 실패 시에만 random_device 로 폴백한다.
void fill_random(unsigned char* out, size_t n)
{
#ifndef _WIN32
    // POSIX: /dev/urandom 에서 직접 읽는다(글리브C random_device 와 동일 소스지만 명시적).
    if (FILE* f = std::fopen("/dev/urandom", "rb")) {
        size_t got = std::fread(out, 1, n, f);
        std::fclose(f);
        if (got == n) return;
    }
#endif
    // 폴백(Windows 포함): random_device.
    std::random_device rd;
    size_t i = 0;
    while (i < n) {
        uint32_t x = rd();
        size_t take = (n - i < 4) ? (n - i) : 4;
        std::memcpy(out + i, &x, take);
        i += take;
    }
}

// 32 hex chars 무작위 토큰 (16 바이트 = 128비트 엔트로피).
std::string gen_token()
{
    unsigned char raw[16];
    fill_random(raw, sizeof(raw));
    static const char hex[] = "0123456789abcdef";
    char buf[33];
    for (int i = 0; i < 16; ++i) {
        buf[i * 2]     = hex[(raw[i] >> 4) & 0xF];
        buf[i * 2 + 1] = hex[raw[i] & 0xF];
    }
    buf[32] = '\0';
    return std::string(buf, 32);
}

// [보안] 상수 시간 문자열 비교(타이밍 사이드채널 방지).
//   내용에 따라 조기 종료/분기하지 않는다. 길이가 다르면 false.
bool ct_equal(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    volatile unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

bool parse_int_param(const std::string& s, int& out)
{
    if (s.empty()) return false;
    int value = 0;
    auto* first = s.data();
    auto* last = s.data() + s.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last) return false;
    out = value;
    return true;
}

// CORS + content-type 을 한 번에 세팅.
void set_json(httplib::Response& res, int status, const std::string& body)
{
    res.status = status;
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, X-Relay-Secret");
    res.set_content(body, "application/json");
}

} // namespace

ApiServer::ApiServer(Database& db, std::string relay_secret)
    : db_(db), relay_secret_(std::move(relay_secret)) {}

bool ApiServer::listen(const std::string& host, int port)
{
    httplib::Server svr;

    // [보안] 요청 본문 상한 — 거대한 body 로 메모리를 소모시키는 플러딩 방지.
    //   우리 엔드포인트의 정상 body 는 수백 바이트 수준이라 64KiB 면 충분.
    svr.set_payload_max_length(64 * 1024);

    // [보안] 간단한 per-IP 고정 윈도우 레이트 리밋(남용/DoS 완화).
    //   1초 창에 IP 당 kMaxPerWindow 초과 요청은 429. 창 전환 시 맵을 비워
    //   메모리 증가를 막는다(고정 윈도우라 경계에서 최대 2배 burst 허용 — 충분).
    svr.set_pre_routing_handler(
        [](const httplib::Request& req, httplib::Response& res) {
            static std::mutex mu;
            static std::unordered_map<std::string, int> hits;
            static int64_t window = 0;
            constexpr int kMaxPerWindow = 60;
            const int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            std::lock_guard<std::mutex> lk(mu);
            if (nowSec != window) { window = nowSec; hits.clear(); }
            if (++hits[req.remote_addr] > kMaxPerWindow) {
                res.status = 429;
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("{\"error\":\"rate_limited\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });

    // ------- CORS preflight (브라우저 정적 페이지용) ------------------------
    svr.Options(R"(/v1/.*)",
        [](const httplib::Request&, httplib::Response& res) {
            res.status = 204;
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, X-Relay-Secret");
        });

    // ------- GET /healthz ---------------------------------------------------
    svr.Get("/healthz",
        [](const httplib::Request&, httplib::Response& res) {
            set_json(res, 200, "{\"ok\":true}");
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
                    set_json(res, 200, proto::guest_response(
                        p->id, p->token, p->elo, p->bp, p->selected_icon_id));
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
                proto::auth_response(p->id, p->username, p->elo,
                                     p->bp, p->selected_icon_id));
        });

    // ------- GET /v1/icons/catalog -----------------------------------------
    svr.Get("/v1/icons/catalog",
        [this](const httplib::Request&, httplib::Response& res) {
            auto icons = db_.iconCatalog();
            std::vector<proto::IconRow> out;
            out.reserve(icons.size());
            for (const auto& icon : icons) {
                out.push_back({icon.id, icon.name, icon.price_bp, icon.default_owned});
            }
            set_json(res, 200, proto::icon_catalog_response(out));
        });

    // ------- POST /v1/icons/buy --------------------------------------------
    svr.Post("/v1/icons/buy",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string token = proto::find_string(req.body, "token");
            std::string icon  = proto::find_string(req.body, "icon_id");
            if (token.empty() || icon.empty()) {
                set_json(res, 400, proto::error_json("bad_request", "missing token or icon_id"));
                return;
            }
            std::optional<Player> p;
            switch (db_.purchaseIcon(token, icon, p)) {
            case IconPurchaseResult::Ok:
                set_json(res, 200, proto::auth_response(
                    p->id, p->username, p->elo, p->bp, p->selected_icon_id));
                return;
            case IconPurchaseResult::UnknownToken:
                set_json(res, 404, proto::error_json("unknown_token")); return;
            case IconPurchaseResult::InvalidIcon:
                set_json(res, 400, proto::error_json("invalid_icon")); return;
            case IconPurchaseResult::AlreadyOwned:
                set_json(res, 409, proto::error_json("already_owned")); return;
            case IconPurchaseResult::InsufficientBp:
                set_json(res, 402, proto::error_json("insufficient_bp")); return;
            case IconPurchaseResult::DbError:
            default:
                set_json(res, 500, proto::error_json("db_error")); return;
            }
        });

    // ------- POST /v1/icons/select -----------------------------------------
    svr.Post("/v1/icons/select",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string token = proto::find_string(req.body, "token");
            std::string icon  = proto::find_string(req.body, "icon_id");
            if (token.empty() || icon.empty()) {
                set_json(res, 400, proto::error_json("bad_request", "missing token or icon_id"));
                return;
            }
            std::optional<Player> p;
            switch (db_.selectIcon(token, icon, p)) {
            case IconSelectResult::Ok:
                set_json(res, 200, proto::auth_response(
                    p->id, p->username, p->elo, p->bp, p->selected_icon_id));
                return;
            case IconSelectResult::UnknownToken:
                set_json(res, 404, proto::error_json("unknown_token")); return;
            case IconSelectResult::InvalidIcon:
                set_json(res, 400, proto::error_json("invalid_icon")); return;
            case IconSelectResult::NotOwned:
                set_json(res, 403, proto::error_json("not_owned")); return;
            case IconSelectResult::DbError:
            default:
                set_json(res, 500, proto::error_json("db_error")); return;
            }
        });

    // ------- POST /v1/matches ----------------------------------------------
    svr.Post("/v1/matches",
        [this](const httplib::Request& req, httplib::Response& res) {
            if (!relay_secret_.empty() &&
                !ct_equal(req.get_header_value("X-Relay-Secret"), relay_secret_)) {
                set_json(res, 403, proto::error_json("forbidden", "relay secret required"));
                return;
            }

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
                int parsed = 20;
                if (parse_int_param(req.get_param_value("limit"), parsed)) {
                    limit = parsed;
                }
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
