#pragma once
#include "http_client.h"
#include "json_input.h"
#include "credentials.h"
#include <limits>
#include <utility>

namespace meta::client::profile_response {
// Read from an already validated object. Range checks precede narrowing.
inline std::optional<int64_t> nonnegative(const json_input::Json& j,
                                         const char* key, int64_t maximum) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) return std::nullopt;
    if (it->is_number_unsigned()) {
        const auto value = it->get<uint64_t>();
        if (value > static_cast<uint64_t>(maximum)) return std::nullopt;
        return static_cast<int64_t>(value);
    }
    const auto value = it->get<int64_t>();
    if (value < 0 || value > maximum) return std::nullopt;
    return value;
}
inline std::optional<AuthInfo> fields(const json_input::Json& j) {
    if (!j.is_object()) return std::nullopt;
    const auto id = nonnegative(j, "player_id", std::numeric_limits<int64_t>::max());
    const auto elo = nonnegative(j, "elo", std::numeric_limits<int>::max());
    const auto bp = nonnegative(j, "bp", std::numeric_limits<int>::max());
    // Legacy compatibility applies only to an absent XP field, not malformed XP.
    const auto xp = j.contains("xp") ? nonnegative(j, "xp", std::numeric_limits<int>::max())
                                    : std::optional<int64_t>{0};
    const auto icon = j.find("selected_icon_id");
    if (!id || *id == 0 || !elo || !bp || !xp || icon == j.end() ||
        !icon->is_string() || icon->get_ref<const std::string&>().empty()) return std::nullopt;
    std::string username;
    const auto name = j.find("username");
    if (name != j.end() && !name->is_null()) {
        if (!name->is_string()) return std::nullopt;
        username = name->get<std::string>();
    }
    return AuthInfo{*id, std::move(username), static_cast<int>(*elo), static_cast<int>(*bp),
                    static_cast<int>(*xp), icon->get<std::string>()};
}
inline std::optional<AuthInfo> auth(const std::string& body) {
    const auto j = json_input::object(body);
    return j ? fields(*j) : std::nullopt;
}
inline std::optional<GuestInfo> guest(const std::string& body) {
    const auto j = json_input::object(body);
    if (!j) return std::nullopt;
    const auto profile = fields(*j);
    const auto token = j->find("token");
    if (!profile || token == j->end() || !token->is_string() ||
        !credentials::account(token->get_ref<const std::string&>())) return std::nullopt;
    return GuestInfo{profile->player_id, token->get<std::string>(), profile->elo,
                     profile->bp, profile->xp, profile->selected_icon_id};
}
} // namespace meta::client::profile_response
