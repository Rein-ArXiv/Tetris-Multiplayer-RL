#include "http_client.h"
#include <algorithm>
#include <cctype>
#include "protocol.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
#endif

// httplib 는 헤더 온리라 여기서 한 번만 포함해 impl 을 생성한다. game client /
// relay 양쪽에서 이 .cpp 가 링크되면 httplib 심볼이 중복되므로, 상위 CMake 는
// 이 파일을 한 타겟당 한 번만 추가해야 한다.
#include "httplib.h"

#include <cstdlib>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <utility>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

namespace meta::client {

namespace {

bool parse_port(const std::string& s, int& out)
{
    if (s.empty()) return false;
    int value = 0;
    auto* first = s.data();
    auto* last = s.data() + s.size();
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last) return false;
    if (value < 1 || value > 65535) return false;
    out = value;
    return true;
}

// API URLs identify an origin, not an arbitrary path. Reject userinfo, queries,
// fragments and hidden suffixes so storage identity matches the actual destination.
bool parse_meta_url(const std::string& url, std::string& host, int& port, bool& https)
{
    std::string text = url;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    size_t prefix = 0;
    if (text.rfind("https://", 0) == 0) { https = true; port = 443; prefix = 8; }
    else if (text.rfind("http://", 0) == 0) { https = false; port = 80; prefix = 7; }
    else return false;
    std::string authority = text.substr(prefix);
    if (!authority.empty() && authority.back() == '/') authority.pop_back();
    if (authority.empty() || authority.find_first_of("/@?#%\\ \t\r\n") != std::string::npos) return false;
    if (authority.front() == '[') {
        const auto end = authority.find(']');
        if (end == std::string::npos) return false;
        host = authority.substr(1, end - 1);
        if (host.empty() || host.find_first_not_of("0123456789abcdef:.") != std::string::npos) return false;
        const auto suffix = authority.substr(end + 1);
        return suffix.empty() || (suffix.front() == ':' && parse_port(suffix.substr(1), port));
    }
    const auto colon = authority.find(':');
    host = authority.substr(0, colon);
    if (host.empty() || host.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.-") != std::string::npos) return false;
    return colon == std::string::npos || parse_port(authority.substr(colon + 1), port);
}

template <typename ClientT>
void configure_client(ClientT& cli, int timeout_s)
{
    cli.set_connection_timeout(timeout_s, 0);
    cli.set_read_timeout      (timeout_s, 0);
    cli.set_write_timeout     (timeout_s, 0);
}

std::optional<AuthInfo> parse_auth_info_body(const std::string& body)
{
    auto pid = proto::find_int   (body, "player_id");
    auto uname = proto::find_string(body, "username");  // null 이면 ""
    auto elo = proto::find_int   (body, "elo");
    auto bp  = proto::find_int   (body, "bp");
    auto xp  = proto::find_int   (body, "xp");          // 구 서버 응답엔 없음 → 0
    auto icon = proto::find_string(body, "selected_icon_id");
    if (!pid || !elo || !bp || icon.empty()) return std::nullopt;
    return AuthInfo{ *pid, std::move(uname), static_cast<int>(*elo),
                     static_cast<int>(*bp), static_cast<int>(xp.value_or(0)),
                     std::move(icon) };
}

} // namespace

// -----------------------------------------------------------------------------
MetaClient::MetaClient(const std::string& base_url, std::string relay_secret)
    : base_url_(base_url), relay_secret_(std::move(relay_secret))
{
    const char* legacy = std::getenv("TETRIS_RELAY_LEGACY_AUTH");
    if (!relay_secret_.empty() && legacy && std::string(legacy) == "1") {
        std::fprintf(stderr, "[meta-client] WARNING: legacy account-token admission enabled; use only for migration tests\n");
    }
    valid_ = parse_meta_url(base_url, host_, port_, https_);
    if (valid_ && relay_secret_.empty() && !https_ &&
        host_ != "127.0.0.1" && host_ != "::1" && host_ != "localhost") {
        valid_ = false; // ordinary clients never send account credentials over remote HTTP
    }
    if (valid_) {
        const auto authority = host_.find(':') == std::string::npos ? host_ : "[" + host_ + "]";
        base_url_ = std::string(https_ ? "https://" : "http://") + authority;
        if (port_ != (https_ ? 443 : 80)) base_url_ += ":" + std::to_string(port_);
    }
    if (!valid_) {
        std::fprintf(stderr, "[meta-client] invalid URL: %s\n", base_url.c_str());
        return;
    }
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
    if (https_) {
        valid_ = false;
        std::fprintf(stderr,
                     "[meta-client] HTTPS URL requires OpenSSL build support: %s\n",
                     base_url.c_str());
    }
#endif
}

namespace {

httplib::Result post_json(const MetaClient& mc, const std::string& host, int port,
                          bool https, const char* path, const httplib::Headers& headers,
                          const std::string& body, int timeout_s)
{
    (void)mc;
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (https) {
        httplib::SSLClient cli(host, port);
        cli.enable_server_certificate_verification(true);
        if (const char* ca = std::getenv("TETRIS_CA_FILE")) cli.set_ca_cert_path(ca);
        configure_client(cli, timeout_s);
        return cli.Post(path, headers, body, "application/json");
    }
#else
    (void)https;
#endif
    httplib::Client cli(host, port);
    configure_client(cli, timeout_s);
    return cli.Post(path, headers, body, "application/json");
}

httplib::Result get_path(const std::string& host, int port, bool https,
                         const char* path, int timeout_s)
{
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (https) {
        httplib::SSLClient cli(host, port);
        cli.enable_server_certificate_verification(true);
        if (const char* ca = std::getenv("TETRIS_CA_FILE")) cli.set_ca_cert_path(ca);
        configure_client(cli, timeout_s);
        return cli.Get(path);
    }
#else
    (void)https;
#endif
    httplib::Client cli(host, port);
    configure_client(cli, timeout_s);
    return cli.Get(path);
}

} // namespace

std::optional<GuestInfo>
MetaClient::request_guest(int timeout_s)
{
    if (!valid_) return std::nullopt;
    auto r = post_json(*this, host_, port_, https_, "/v1/guest", {}, "{}", timeout_s);
    if (!r) {
        std::fprintf(stderr, "[meta-client] /v1/guest network error\n");
        return std::nullopt;
    }
    if (r->status != 200) {
        std::fprintf(stderr, "[meta-client] /v1/guest HTTP %d: %s\n",
                     r->status, r->body.c_str());
        return std::nullopt;
    }
    auto pid = proto::find_int   (r->body, "player_id");
    auto tok = proto::find_string(r->body, "token");
    auto elo = proto::find_int   (r->body, "elo");
    auto bp  = proto::find_int   (r->body, "bp");
    auto xp  = proto::find_int   (r->body, "xp");
    auto icon = proto::find_string(r->body, "selected_icon_id");
    if (!pid || tok.empty() || !elo || !bp || icon.empty()) {
        std::fprintf(stderr, "[meta-client] /v1/guest bad response\n");
        return std::nullopt;
    }
    return GuestInfo{ *pid, std::move(tok), static_cast<int>(*elo),
                      static_cast<int>(*bp), static_cast<int>(xp.value_or(0)),
                      std::move(icon) };
}

std::optional<AuthInfo>
MetaClient::verify_token(const std::string& token, int timeout_s,
                         VerifyOutcome* out_outcome)
{
    auto set_outcome = [&](VerifyOutcome o) { if (out_outcome) *out_outcome = o; };

    if (!valid_)        { set_outcome(VerifyOutcome::NetworkError); return std::nullopt; }
    if (token.empty())  { set_outcome(VerifyOutcome::UnknownToken); return std::nullopt; }

    std::string body = std::string("{\"token\":\"") + proto::json_escape(token) + "\"}";
    httplib::Headers headers;
    if (!relay_secret_.empty()) {
        headers.emplace("X-Relay-Secret", relay_secret_);
    }
    auto r = post_json(*this, host_, port_, https_, "/v1/auth/verify", headers,
                       body, timeout_s);
    if (!r) {
        std::fprintf(stderr, "[meta-client] /v1/auth/verify network error\n");
        set_outcome(VerifyOutcome::NetworkError);
        return std::nullopt;
    }
    if (r->status == 404) {
        // 토큰 미등록 — 호출자가 복구를 안내하고 새 입장을 거절한다.
        set_outcome(VerifyOutcome::UnknownToken);
        return std::nullopt;
    }
    if (r->status != 200) {
        std::fprintf(stderr, "[meta-client] /v1/auth/verify HTTP %d: %s\n",
                     r->status, r->body.c_str());
        // 5xx 등은 일시적 — 네트워크 오류로 분류해 토큰을 그대로 두고 재시도.
        set_outcome(VerifyOutcome::NetworkError);
        return std::nullopt;
    }
    auto parsed = parse_auth_info_body(r->body);
    if (!parsed) {
        std::fprintf(stderr, "[meta-client] /v1/auth/verify bad response\n");
        set_outcome(VerifyOutcome::NetworkError);
        return std::nullopt;
    }
    set_outcome(VerifyOutcome::Ok);
    return parsed;
}

std::optional<std::string> MetaClient::request_game_ticket(const std::string& token)
{
    // Plain HTTP is permitted only for explicit local development. A remote meta
    // URL must not leak the long-lived account credential while issuing a ticket.
    if (!valid_ || (!https_ && host_ != "127.0.0.1" && host_ != "::1" && host_ != "localhost")) return std::nullopt;
    auto r = post_json(*this, host_, port_, https_, "/v1/game-tickets", {},
                       "{\"token\":\"" + proto::json_escape(token) + "\"}", 5);
    if (!r || r->status != 200) return std::nullopt;
    auto ticket = proto::find_string(r->body, "ticket");
    if (ticket.size() != 36 || ticket.substr(0,4) != "gt1." ||
        ticket.find_first_not_of("0123456789abcdef",4) != std::string::npos) return std::nullopt;
    return ticket;
}

std::optional<AuthInfo> MetaClient::consume_game_ticket(const std::string& ticket)
{
    if (!valid_ || relay_secret_.empty()) return std::nullopt;
    // Explicit local compatibility switch for pre-ticket clients/tests. Never
    // enabled in release services. Ticket redemption NEVER takes this path.
    if (ticket.rfind("gt1.",0) != 0) {
        const char* legacy = std::getenv("TETRIS_RELAY_LEGACY_AUTH");
        if (legacy && std::string(legacy) == "1") return verify_token(ticket);
        return std::nullopt;
    }
    auto r = post_json(*this, host_, port_, https_, "/v1/game-tickets/consume",
                       {{"X-Relay-Secret", relay_secret_}},
                       "{\"ticket\":\"" + proto::json_escape(ticket) + "\"}", 3);
    if (!r || r->status != 200) return std::nullopt;
    return parse_auth_info_body(r->body);
}

std::optional<std::vector<IconEntry>>
MetaClient::fetch_icon_catalog(int timeout_s)
{
    if (!valid_) return std::nullopt;
    auto r = get_path(host_, port_, https_, "/v1/icons/catalog", timeout_s);
    if (!r || r->status != 200) {
        std::fprintf(stderr, "[meta-client] /v1/icons/catalog %s\n",
                     r ? "HTTP error" : "network error");
        return std::nullopt;
    }
    // 응답은 평탄한 객체들의 배열: [{"id":..,"name":..,"price_bp":..,"default_owned":..}, ...]
    // 행 내부엔 중첩 객체가 없지만, name 문자열에 '}' 가 들어가도 깨지지 않도록
    // 문자열 리터럴(따옴표, 백슬래시 이스케이프)을 건너뛰며 매칭 '}' 를 찾는다.
    std::vector<IconEntry> out;
    const std::string& body = r->body;
    size_t pos = 0;
    while ((pos = body.find('{', pos)) != std::string::npos) {
        size_t end = std::string::npos;
        bool inStr = false;
        for (size_t i = pos + 1; i < body.size(); ++i) {
            const char c = body[i];
            if (inStr) {
                if (c == '\\') { ++i; continue; }   // 이스케이프된 다음 char 스킵
                if (c == '"')  inStr = false;
            } else if (c == '"') {
                inStr = true;
            } else if (c == '}') {
                end = i; break;
            }
        }
        if (end == std::string::npos) break;
        const std::string obj = body.substr(pos, end - pos + 1);
        pos = end + 1;

        IconEntry e;
        e.id   = proto::find_string(obj, "id");
        e.name = proto::find_string(obj, "name");
        auto price = proto::find_int (obj, "price_bp");
        auto owned = proto::find_bool(obj, "default_owned");
        if (e.id.empty() || !price || !owned) continue;  // 알 수 없는 행은 스킵
        e.price_bp      = static_cast<int>(*price);
        e.default_owned = *owned;
        if (e.name.empty()) e.name = e.id;
        out.push_back(std::move(e));
    }
    if (out.empty()) {
        std::fprintf(stderr, "[meta-client] /v1/icons/catalog empty/bad response\n");
        return std::nullopt;
    }
    return out;
}

std::optional<AuthInfo>
MetaClient::purchase_icon(const std::string& token,
                          const std::string& icon_id,
                          int timeout_s, int* out_http_status)
{
    if (out_http_status) *out_http_status = 0;
    if (!valid_ || token.empty() || icon_id.empty()) return std::nullopt;
    std::string body = std::string("{\"token\":\"") + proto::json_escape(token)
                     + "\",\"icon_id\":\"" + proto::json_escape(icon_id) + "\"}";
    auto r = post_json(*this, host_, port_, https_, "/v1/icons/buy", {},
                       body, timeout_s);
    if (!r) return std::nullopt;
    if (out_http_status) *out_http_status = r->status;
    if (r->status != 200) return std::nullopt;
    return parse_auth_info_body(r->body);
}

std::optional<AuthInfo>
MetaClient::select_icon(const std::string& token,
                        const std::string& icon_id,
                        int timeout_s, int* out_http_status)
{
    if (out_http_status) *out_http_status = 0;
    if (!valid_ || token.empty() || icon_id.empty()) return std::nullopt;
    std::string body = std::string("{\"token\":\"") + proto::json_escape(token)
                     + "\",\"icon_id\":\"" + proto::json_escape(icon_id) + "\"}";
    auto r = post_json(*this, host_, port_, https_, "/v1/icons/select", {},
                       body, timeout_s);
    if (!r) return std::nullopt;
    if (out_http_status) *out_http_status = r->status;
    if (r->status != 200) return std::nullopt;
    return parse_auth_info_body(r->body);
}

std::optional<MatchResult>
MetaClient::post_match(const std::string& match_uuid,
                       int64_t player_a, int64_t player_b,
                       std::optional<int64_t> winner,
                       int score_a, int score_b,
                       int lines_a, int lines_b,
                       int duration_s,
                       int timeout_s)
{
    if (!valid_) return std::nullopt;

    std::ostringstream ss;
    ss << "{"
       << "\"match_uuid\":\"" << proto::json_escape(match_uuid) << "\""
       << ",\"player_a\":" << player_a
       << ",\"player_b\":" << player_b
       << ",\"winner\":";
    if (winner) ss << *winner;
    else        ss << "null";
    ss << ",\"score_a\":" << score_a
       << ",\"score_b\":" << score_b
       << ",\"lines_a\":" << lines_a
       << ",\"lines_b\":" << lines_b
       << ",\"duration_s\":" << duration_s
       << "}";
    std::string body = ss.str();

    httplib::Headers headers;
    if (!relay_secret_.empty()) {
        headers.emplace("X-Relay-Secret", relay_secret_);
    }
    // [예산] 재시도를 포함한 전체 wall-clock 을 timeout_s 로 상한한다.
    // 시도별 타임아웃은 connect/read/write 각각에 걸리므로 한 시도가 그 몇 배로
    // 늘어질 수 있고, 기존처럼 3회를 무조건 돌면 최악 ~9초까지 블로킹돼 매치
    // 종료 흐름이 눈에 띄게 지연됐다. 남은 예산 기준으로 시도별 타임아웃을
    // 줄이고, 예산이 소진되면 재시도를 포기한다 (relay 가 멱등 재전송하므로
    // 여기서 무리하게 기다릴 이유가 없다).
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(std::max(1, timeout_s));
    auto remaining_s = [&]() -> int {
        const auto left = std::chrono::duration_cast<std::chrono::seconds>(
            deadline - std::chrono::steady_clock::now()).count();
        return static_cast<int>(left);
    };

    const int per_attempt_timeout = std::max(1, timeout_s / 3);
    auto r = post_json(*this, host_, port_, https_, "/v1/matches", headers,
                       body, std::min(per_attempt_timeout,
                                      std::max(1, remaining_s())));
    for (int attempt = 1;
         attempt < 3 && (!r || r->status == 429 || r->status >= 500);
         ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * attempt));
        const int left = remaining_s();
        if (left <= 0) {
            std::fprintf(stderr,
                         "[meta-client] /v1/matches retry budget exhausted "
                         "after attempt %d\n", attempt);
            break;
        }
        r = post_json(*this, host_, port_, https_, "/v1/matches", headers,
                      body, std::min(per_attempt_timeout, left));
    }
    if (!r) {
        std::fprintf(stderr, "[meta-client] /v1/matches network error\n");
        return std::nullopt;
    }
    if (r->status != 200) {
        std::fprintf(stderr, "[meta-client] /v1/matches HTTP %d: %s\n",
                     r->status, r->body.c_str());
        return std::nullopt;
    }

    // 응답 파싱 — 중첩된 "a"/"b" 가 있지만 each 는 평면. 서브오브젝트 범위에서
    // find_int 를 호출하려면 수동으로 오프셋을 계산해야 한다.
    auto mid = proto::find_int(r->body, "match_id");
    if (!mid) return std::nullopt;

    auto find_sub = [&](const char* key, std::size_t& start, std::size_t& end) -> bool {
        std::string pat = std::string("\"") + key + "\":{";
        auto i = r->body.find(pat);
        if (i == std::string::npos) return false;
        auto j = r->body.find('}', i);
        if (j == std::string::npos) return false;
        start = i + pat.size();
        end   = j;
        return true;
    };
    auto parse_side = [&](const char* key, MatchDelta& out) -> bool {
        std::size_t s = 0, e = 0;
        if (!find_sub(key, s, e)) return false;
        std::string sub = r->body.substr(s - 1, e - s + 2);  // include "{...}"
        auto bef = proto::find_int(sub, "elo_before");
        auto aft = proto::find_int(sub, "elo_after");
        auto del = proto::find_int(sub, "delta");
        if (!bef || !aft || !del) return false;
        out.elo_before = static_cast<int>(*bef);
        out.elo_after  = static_cast<int>(*aft);
        out.delta      = static_cast<int>(*del);
        return true;
    };
    MatchResult res{};
    res.match_id = *mid;
    if (!parse_side("a", res.a) || !parse_side("b", res.b)) {
        std::fprintf(stderr, "[meta-client] /v1/matches bad response\n");
        return std::nullopt;
    }
    return res;
}

// -----------------------------------------------------------------------------
// Token 파일 헬퍼
// -----------------------------------------------------------------------------

std::optional<AuthInfo> MetaClient::change_account(const std::string& operation,
    const std::string& credential,const std::string& next_token,const std::string& next_recovery,int* status)
{
    if(status)*status=0;
    if(!valid_ || (operation!="backup" && operation!="rotate" && operation!="recover"))return std::nullopt;
    const auto body="{\"credential\":\""+proto::json_escape(credential)+"\",\"next_token\":\""+
        proto::json_escape(next_token)+"\",\"next_recovery\":\""+proto::json_escape(next_recovery)+"\"}";
    const auto path="/v1/account/"+operation;
    auto response=post_json(*this,host_,port_,https_,path.c_str(),{},body,5);
    if(!response)return std::nullopt;
    if(status)*status=response->status;
    if(response->status!=200)return std::nullopt;
    return parse_auth_info_body(response->body);
}


std::optional<BotChallenge> MetaClient::start_bot_challenge(const std::string& token,const std::string& opponent,int* status) {
    if(status)*status=0;
    if(!valid_)return std::nullopt;
    const auto body="{\"token\":\""+proto::json_escape(token)+"\",\"opponent_id\":\""+proto::json_escape(opponent)+"\"}";
    auto r=post_json(*this,host_,port_,https_,"/v1/bots/challenge",{},body,5);
    if(!r)return std::nullopt;
    if(status)*status=r->status;
    if(r->status!=200)return std::nullopt;
    auto id=proto::find_string(r->body,"ticket");
    auto seed=proto::find_int(r->body,"seed"), interval=proto::find_int(r->body,"input_ticks"), think=proto::find_int(r->body,"think_ticks"), minimum=proto::find_int(r->body,"min_piece_ticks");
    if(id.size()!=32 || !seed || *seed<0 || !interval || *interval<1 || *interval>30 || !think || *think<0 || *think>180 || !minimum || *minimum<1 || *minimum>600)return std::nullopt;
    return BotChallenge{id,static_cast<uint64_t>(*seed),static_cast<int>(*interval),static_cast<int>(*think),static_cast<int>(*minimum)};
}
std::optional<BotReward> MetaClient::claim_bot_reward(const std::string& token,const std::string& ticket,const std::string& inputs,int* status) {
    if(status)*status=0;
    if(!valid_)return std::nullopt;
    const auto body="{\"token\":\""+proto::json_escape(token)+"\",\"ticket\":\""+proto::json_escape(ticket)+"\",\"inputs_hex\":\""+proto::json_escape(inputs)+"\"}";
    auto r=post_json(*this,host_,port_,https_,"/v1/bots/claim",{},body,10);
    if(!r)return std::nullopt;
    if(status)*status=r->status;
    if(r->status!=200)return std::nullopt;
    auto earned=proto::find_int(r->body,"awarded_bp"),bp=proto::find_int(r->body,"bp");
    if(!earned || *earned<0 || *earned>10 || !bp || *bp<0 || *bp>2147483647)return std::nullopt;
    return BotReward{static_cast<int>(*earned),static_cast<int>(*bp)};
}

} // namespace meta::client
