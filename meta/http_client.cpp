#include "http_client.h"
#include "protocol.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shlobj.h>
#endif

// httplib 는 헤더 온리라 여기서 한 번만 포함해 impl 을 생성한다. game client /
// relay 양쪽에서 이 .cpp 가 링크되면 httplib 심볼이 중복되므로, 상위 CMake 는
// 이 파일을 한 타겟당 한 번만 추가해야 한다.
#include "httplib.h"

#include <cstdlib>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>
#include <sstream>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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

// URL 파서 — "http://host[:port]" / "https://host[:port]" 허용.
bool parse_meta_url(const std::string& url, std::string& host, int& port, bool& https)
{
    const std::string httpScheme = "http://";
    const std::string httpsScheme = "https://";
    std::string rest;
    if (url.compare(0, httpScheme.size(), httpScheme) == 0) {
        https = false;
        port = 80;
        rest = url.substr(httpScheme.size());
    } else if (url.compare(0, httpsScheme.size(), httpsScheme) == 0) {
        https = true;
        port = 443;
        rest = url.substr(httpsScheme.size());
    } else {
        return false;
    }

    if (rest.empty()) return false;

    // 옵션 경로(/...)가 따라오면 잘라낸다 — 우리 클라이언트는 호스트만 필요.
    auto slash = rest.find('/');
    std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);

    auto colon = hostport.rfind(':');
    if (colon == std::string::npos) {
        host = hostport;
    } else {
        host = hostport.substr(0, colon);
        if (!parse_port(hostport.substr(colon + 1), port)) return false;
    }
    return !host.empty();
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
    valid_ = parse_meta_url(base_url, host_, port_, https_);
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
    auto r = post_json(*this, host_, port_, https_, "/v1/auth/verify", {},
                       body, timeout_s);
    if (!r) {
        std::fprintf(stderr, "[meta-client] /v1/auth/verify network error\n");
        set_outcome(VerifyOutcome::NetworkError);
        return std::nullopt;
    }
    if (r->status == 404) {
        // 토큰 미등록 — 호출자가 새 guest 재발급 또는 매치 입장 거부.
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
MetaClient::post_match(int64_t player_a, int64_t player_b,
                       std::optional<int64_t> winner,
                       int score_a, int score_b,
                       int lines_a, int lines_b,
                       int duration_s,
                       int timeout_s)
{
    if (!valid_) return std::nullopt;

    std::ostringstream ss;
    ss << "{"
       << "\"player_a\":" << player_a
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
    auto r = post_json(*this, host_, port_, https_, "/v1/matches", headers,
                       body, timeout_s);
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

namespace {

// 표준 user-data 디렉토리 기반 경로. 실패 시 빈 문자열.
std::filesystem::path user_data_dir()
{
    namespace fs = std::filesystem;
#ifdef _WIN32
    // %APPDATA% (예: C:\Users\Name\AppData\Roaming)
    char buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        return fs::path(buf);
    }
    const char* appdata = std::getenv("APPDATA");
    if (appdata && *appdata) return fs::path(appdata);
    return {};
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home || !*home) return {};
    return fs::path(home) / "Library" / "Application Support";
#else
    // Linux / other unix
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return fs::path(xdg);
    const char* home = std::getenv("HOME");
    if (!home || !*home) return {};
    return fs::path(home) / ".local" / "share";
#endif
}

} // namespace

std::string token_file_path()
{
    auto base = user_data_dir();
    if (base.empty()) return {};
    return (base / "Tetris" / "token").string();
}

std::string settings_file_path()
{
    auto base = user_data_dir();
    if (base.empty()) return {};
    return (base / "Tetris" / "settings.cfg").string();
}

std::string load_token()
{
    auto path = token_file_path();
    if (path.empty()) return {};

    std::ifstream f(path);
    if (!f) return {};
    std::string tok;
    f >> tok;
    // 32 hex chars 만 허용 — 외부 오염된 파일은 무시.
    if (tok.size() != 32) return {};
    for (char c : tok) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return {};
    }
    return tok;
}

bool save_token(const std::string& token)
{
    namespace fs = std::filesystem;
    auto path = token_file_path();
    if (path.empty()) return false;

    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

#ifndef _WIN32
    const std::string line = token + "\n";
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    if (::fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
        ::close(fd);
        return false;
    }
    size_t written = 0;
    while (written < line.size()) {
        ssize_t n = ::write(fd, line.data() + written, line.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return false;
        }
        if (n == 0) {
            ::close(fd);
            return false;
        }
        written += static_cast<size_t>(n);
    }
    bool ok = (::close(fd) == 0);
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
    return ok;
#else
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << token << "\n";
    bool ok = static_cast<bool>(f);
    f.close();
    fs::permissions(path,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace,
                    ec);
    return ok;
#endif
}

} // namespace meta::client
