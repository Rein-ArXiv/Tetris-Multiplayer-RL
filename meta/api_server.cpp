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
#ifdef _WIN32
  #include <bcrypt.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
#endif

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace meta {

namespace {

// 인증 토큰은 플랫폼 CSPRNG에서만 만든다. 엔트로피 소스가 실패했을 때
// random_device나 시간값으로 폴백하면 "서비스 가용" 상태처럼 보이면서 예측 가능한
// 토큰을 발급할 수 있다. 이 경우 guest 요청 자체를 실패-폐쇄하는 편이 안전하다.
bool fill_random(unsigned char* out, size_t n)
{
#ifdef _WIN32
    if (n > static_cast<size_t>(std::numeric_limits<ULONG>::max())) return false;
    return BCryptGenRandom(nullptr, out, static_cast<ULONG>(n),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    // Linux와 macOS에서 공통으로 쓸 수 있는 커널 난수 장치를 직접 읽는다.
    // read는 요청한 길이보다 짧게 성공할 수 있고 signal에 끊길 수도 있으므로
    // 한 번의 호출 결과를 토큰 전체로 착각하지 않는다.
    int flags = O_RDONLY;
    #ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
    #endif
    const int fd = ::open("/dev/urandom", flags);
    if (fd < 0) return false;
    size_t done = 0;
    while (done < n) {
        const ssize_t got = ::read(fd, out + done, n - done);
        if (got > 0) {
            done += static_cast<size_t>(got);
            continue;
        }
        if (got < 0 && errno == EINTR) continue;
        break;
    }
    ::close(fd);
    return done == n;
#endif
}

// 32 hex chars 무작위 토큰 (16 바이트 = 128비트 엔트로피).
std::optional<std::string> gen_token()
{
    unsigned char raw[16];
    if (!fill_random(raw, sizeof(raw))) return std::nullopt;
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

bool valid_match_uuid(const std::string& s)
{
    if (s.size() != 32) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

// 전달 헤더는 같은 호스트의 loopback 프록시에서만 신뢰한다. 별도 호스트의
// 프록시를 자동으로 신뢰하면 같은 LAN에서 직접 붙은 클라이언트가 XFF를 위조해
// 버킷을 우회할 수 있다. 소형 리눅스 프록시 → 저전력 Android(Termux) meta 같은
// 분리 배치에서는 모든 요청이 proxy IP 버킷을 공유하며, 실제 client별 제한은
// edge가 맡아야 한다.
std::string rate_limit_key(const httplib::Request& req)
{
    const bool from_loopback =
        req.remote_addr == "127.0.0.1" || req.remote_addr == "::1";
    if (from_loopback) {
        std::string ip = req.get_header_value("CF-Connecting-IP");
        if (ip.empty()) {
            // [보안] XFF 는 "client, proxy1, proxy2, ..." 순서로, 경유하는
            // 프록시가 자기 앞단의 주소를 **뒤에 append** 한다. 즉 첫 토큰은
            // 클라이언트가 요청에 미리 심어 위조할 수 있는 값이고(매 요청
            // 다른 값을 넣으면 60/s 공개 버킷을 무한 우회), 신뢰할 수 있는
            // 것은 우리가 믿는 프록시가 마지막에 붙인 rightmost 토큰뿐이다.
            // 따라서 첫 토큰이 아니라 마지막 토큰을 rate limit 키로 쓴다.
            ip = req.get_header_value("X-Forwarded-For");
            const auto comma = ip.rfind(',');
            if (comma != std::string::npos) ip.erase(0, comma + 1);
        }
        const auto b = ip.find_first_not_of(" \t");
        const auto e = ip.find_last_not_of(" \t");
        if (b != std::string::npos) return ip.substr(b, e - b + 1);
    }
    return req.remote_addr;
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

    // Normal requests are only a few hundred bytes.
    svr.set_payload_max_length(64 * 1024);

    // Bounded one-second request window per client IP.
    svr.set_pre_routing_handler(
        [this](const httplib::Request& req, httplib::Response& res) {
            static std::mutex mu;
            static std::unordered_map<std::string, int> hits;
            static int64_t window = 0;
            const bool trustedRelay = !relay_secret_.empty() &&
                ct_equal(req.get_header_value("X-Relay-Secret"), relay_secret_);
            const int maxPerWindow = trustedRelay ? 512 : 60;
            const int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            std::lock_guard<std::mutex> lk(mu);
            if (nowSec != window) { window = nowSec; hits.clear(); }
            const std::string key = (trustedRelay ? "relay:" : "public:") +
                                    rate_limit_key(req);
            if (++hits[key] > maxPerWindow) {
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
                if (!token) {
                    std::fprintf(stderr, "[meta] OS CSPRNG unavailable; refusing guest token\n");
                    set_json(res, 500,
                             proto::error_json("entropy_unavailable",
                                               "secure token generation failed"));
                    return;
                }
                auto p = db_.registerGuest(*token);
                if (p) {
                    set_json(res, 200, proto::guest_response(
                        p->id, p->token, p->elo, p->bp, p->xp,
                        p->selected_icon_id));
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
                                     p->bp, p->xp, p->selected_icon_id));
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
                    p->id, p->username, p->elo, p->bp, p->xp,
                    p->selected_icon_id));
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
                    p->id, p->username, p->elo, p->bp, p->xp,
                    p->selected_icon_id));
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

            const std::string matchUuid = proto::find_string(req.body, "match_uuid");
            auto pa = proto::find_int(req.body, "player_a");
            auto pb = proto::find_int(req.body, "player_b");
            auto wn = proto::find_int(req.body, "winner");   // null 허용
            auto sa = proto::find_int(req.body, "score_a");
            auto sb = proto::find_int(req.body, "score_b");
            auto la = proto::find_int(req.body, "lines_a");
            auto lb = proto::find_int(req.body, "lines_b");
            auto du = proto::find_int(req.body, "duration_s");

            if (!valid_match_uuid(matchUuid) || !pa || !pb || !sa || !sb || !la || !lb || !du) {
                set_json(res, 400,
                    proto::error_json("bad_request", "invalid match_uuid or missing fields"));
                return;
            }
            if (*pa == *pb) {
                set_json(res, 400,
                    proto::error_json("bad_request", "player_a == player_b"));
                return;
            }
            // A winner must identify one side of this match.
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
            // Validate before narrowing int64 JSON values to int.
            constexpr int64_t kMaxStatValue = 100000000;
            if (*sa > kMaxStatValue || *sb > kMaxStatValue ||
                *la > kMaxStatValue || *lb > kMaxStatValue ||
                *du > kMaxStatValue) {
                set_json(res, 400,
                    proto::error_json("bad_request", "scores/lines/duration out of range"));
                return;
            }

            MatchRecord m;
            m.match_uuid = matchUuid;
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
                out.push_back({ r.player_id, r.username, r.elo, r.wins,
                                r.losses, r.xp });
            }
            set_json(res, 200, proto::leaderboard_response(out));
        });

    std::fprintf(stderr, "[meta] HTTP listening on %s:%d\n", host.c_str(), port);
    bool ok = svr.listen(host, port);
    if (!ok) std::fprintf(stderr, "[meta] listen failed on %s:%d\n", host.c_str(), port);
    return ok;
}

} // namespace meta
