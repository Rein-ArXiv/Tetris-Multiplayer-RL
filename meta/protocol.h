#pragma once

// meta/protocol.h — JSON 수동 직렬화/파싱 헬퍼.
//
// 우리 엔드포인트들은 대부분 "평면적인 primitive 필드"로만 구성되므로
// nlohmann/json 같은 풀스펙 라이브러리는 과하다. 이 헤더의 함수들은
// 특정 응답 shape 마다 전용으로 만들어져 있어 읽기 쉽고 빠르다.
//
// 응답 규약:
//   · 200: 엔드포인트별 페이로드
//   · 4xx/5xx: {"error":"...","reason":"..."}
//
// 파싱 규약: Content-Type 무시하고 body 에서 원하는 키를 substr+find 로 뽑는다.
// malformed JSON 은 find_int/find_string 이 -1 또는 빈 문자열 반환 → 호출자가 400.

#include <cstdint>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "levels.h"   // xp -> level 유도 (응답에 level 을 함께 실어준다)

namespace meta::proto {

// --- JSON 문자열 escape (쌍따옴표/백슬래시/제어문자만. UTF-8 그대로 통과) -----
inline std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// --- 응답 빌더 ----------------------------------------------------------------

inline std::string error_json(const char* err, const char* reason = nullptr)
{
    std::ostringstream ss;
    ss << "{\"error\":\"" << err << "\"";
    if (reason) ss << ",\"reason\":\"" << json_escape(reason) << "\"";
    ss << "}";
    return ss.str();
}

// POST /v1/guest 응답
inline std::string guest_response(int64_t player_id,
                                  const std::string& token,
                                  int elo,
                                  int bp,
                                  int xp,
                                  const std::string& selected_icon_id)
{
    std::ostringstream ss;
    ss << "{\"player_id\":" << player_id
       << ",\"token\":\""   << json_escape(token) << "\""
       << ",\"elo\":"       << elo
       << ",\"bp\":"        << bp
       << ",\"xp\":"        << xp
       << ",\"level\":"     << levels::level_for_xp(xp)
       << ",\"selected_icon_id\":\"" << json_escape(selected_icon_id) << "\""
       << "}";
    return ss.str();
}

// POST /v1/auth/verify 응답
inline std::string auth_response(int64_t player_id,
                                 const std::optional<std::string>& username,
                                 int elo,
                                 int bp,
                                 int xp,
                                 const std::string& selected_icon_id)
{
    std::ostringstream ss;
    ss << "{\"player_id\":" << player_id
       << ",\"username\":";
    if (username) ss << "\"" << json_escape(*username) << "\"";
    else          ss << "null";
    ss << ",\"elo\":" << elo
       << ",\"bp\":" << bp
       << ",\"xp\":" << xp
       << ",\"level\":" << levels::level_for_xp(xp)
       << ",\"selected_icon_id\":\"" << json_escape(selected_icon_id) << "\""
       << "}";
    return ss.str();
}

struct IconRow {
    std::string id;
    std::string name;
    int         price_bp;
    bool        default_owned;
};
inline std::string icon_catalog_response(const std::vector<IconRow>& rows)
{
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        ss << "{\"id\":\"" << json_escape(r.id) << "\""
           << ",\"name\":\"" << json_escape(r.name) << "\""
           << ",\"price_bp\":" << r.price_bp
           << ",\"default_owned\":" << (r.default_owned ? "true" : "false")
           << "}";
        if (i + 1 < rows.size()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

// POST /v1/matches 응답 — 양 플레이어의 RP 변동(필드명 elo_* 는 하위 호환용).
struct SideDelta {
    int elo_before;
    int elo_after;
    int delta;
};
inline std::string matches_response(int64_t match_id,
                                    const SideDelta& a, const SideDelta& b)
{
    std::ostringstream ss;
    ss << "{\"match_id\":" << match_id
       << ",\"a\":{\"elo_before\":" << a.elo_before
              << ",\"elo_after\":"  << a.elo_after
              << ",\"delta\":"      << a.delta << "}"
       << ",\"b\":{\"elo_before\":" << b.elo_before
              << ",\"elo_after\":"  << b.elo_after
              << ",\"delta\":"      << b.delta << "}"
       << "}";
    return ss.str();
}

// GET /v1/leaderboard 응답 — rank 는 호출 측에서 enumerate 로 붙임.
struct LeaderRow {
    int64_t     player_id;
    std::optional<std::string> username;
    int         elo;
    int         wins;
    int         losses;
    int         xp;
};
inline std::string leaderboard_response(const std::vector<LeaderRow>& rows)
{
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        ss << "{\"rank\":" << (i + 1)
           << ",\"player_id\":" << r.player_id
           << ",\"username\":";
        if (r.username) ss << "\"" << json_escape(*r.username) << "\"";
        else            ss << "null";
        ss << ",\"elo\":" << r.elo
           << ",\"wins\":" << r.wins
           << ",\"losses\":" << r.losses
           << ",\"level\":" << levels::level_for_xp(r.xp)
           << "}";
        if (i + 1 < rows.size()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

// --- 파싱 헬퍼 (요청 바디) ----------------------------------------------------
//
// nested object 없음, 배열 없음, 주석 없음 가정. 모든 필드는 top-level primitive.
//
// find_string("token")  → `"token"\s*:\s*"VALUE"`  에서 VALUE 반환 (없으면 빈 문자열)
// find_int("player_a")  → 숫자(null 허용) 반환. 없으면 std::nullopt.
// find_bool("won")      → true/false. 없으면 std::nullopt.

namespace detail {

inline size_t skip_ws(const std::string& s, size_t i)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' ||
                            s[i] == '\n' || s[i] == '\r'))
        ++i;
    return i;
}

// key 의 시작 인덱스를 찾아 콜론 뒤까지 이동. 없으면 npos.
inline size_t find_key_colon(const std::string& body, const char* key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = 0;
    while ((pos = body.find(needle, pos)) != std::string::npos) {
        // 콜론까지 이동
        size_t after = skip_ws(body, pos + needle.size());
        if (after < body.size() && body[after] == ':') {
            return skip_ws(body, after + 1);
        }
        pos += needle.size();
    }
    return std::string::npos;
}

} // namespace detail

// key → 문자열 값 (unescape 최소한: \" \\ \n \r \t 만).
inline std::string find_string(const std::string& body, const char* key)
{
    size_t i = detail::find_key_colon(body, key);
    if (i == std::string::npos) return {};
    if (i >= body.size() || body[i] != '"') return {};
    ++i;
    std::string out;
    while (i < body.size() && body[i] != '"') {
        if (body[i] == '\\' && i + 1 < body.size()) {
            switch (body[i + 1]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += body[i + 1];
            }
            i += 2;
        } else {
            out += body[i++];
        }
    }
    return out;
}

// key → 정수 값. null 이면 nullopt. 부호 허용.
inline std::optional<int64_t> find_int(const std::string& body, const char* key)
{
    size_t i = detail::find_key_colon(body, key);
    if (i == std::string::npos) return std::nullopt;
    // null?
    if (body.compare(i, 4, "null") == 0) return std::nullopt;
    // 숫자 파싱
    size_t j = i;
    if (j < body.size() && (body[j] == '-' || body[j] == '+')) ++j;
    if (j >= body.size() || !(body[j] >= '0' && body[j] <= '9')) return std::nullopt;
    int64_t val = 0;
    bool neg = (body[i] == '-');
    if (body[i] == '+' || body[i] == '-') ++i;
    while (i < body.size() && body[i] >= '0' && body[i] <= '9') {
        int d = body[i] - '0';
        // 오버플로 방지: int64 범위를 벗어나는 입력은 파싱 실패(nullopt)로 처리.
        // 필수 숫자 필드라면 상위(api_server)에서 400 으로 거부된다.
        if (val > (INT64_MAX - d) / 10) return std::nullopt;
        val = val * 10 + d;
        ++i;
    }
    return neg ? -val : val;
}

inline std::optional<bool> find_bool(const std::string& body, const char* key)
{
    size_t i = detail::find_key_colon(body, key);
    if (i == std::string::npos) return std::nullopt;
    if (body.compare(i, 4, "true")  == 0) return true;
    if (body.compare(i, 5, "false") == 0) return false;
    return std::nullopt;
}

} // namespace meta::proto
