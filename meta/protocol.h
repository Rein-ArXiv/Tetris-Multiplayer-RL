#pragma once
#include "json_input.h"

// API별 응답 직렬화와 타입 있는 최상위 필드 조회.
// 요청 전체의 문법·중복 키·깊이는 json_input에서 검증한다.
//
// 응답 규약:
//   · 200: 엔드포인트별 페이로드
//   · 4xx/5xx: {"error":"...","reason":"..."}
//
// 유효하지 않은 문서나 잘못된 필드 타입은 빈 문자열/nullopt를 반환한다.
// POST 핸들러는 json_routes에서 문서 검증을 먼저 거친다.

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

// Fields are read only from a fully validated top-level JSON object.
inline std::string find_string(const std::string& body, const char* key) {
    return json_input::string(body, key);
}
inline std::optional<int64_t> find_int(const std::string& body, const char* key) {
    return json_input::integer(body, key);
}
inline std::optional<bool> find_bool(const std::string& body, const char* key) {
    return json_input::boolean(body, key);
}
} // namespace meta::proto
