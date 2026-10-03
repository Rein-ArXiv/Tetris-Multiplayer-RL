#pragma once
#include "http_client.h"
#include "json_input.h"
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace meta::client {
// Parse one complete service response, not substrings found by key spelling.
// Unknown fields are allowed for compatible extensions; required fields keep
// their exact parent, integer type, range and before/after/delta relation.
inline std::optional<MatchResult> parse_match_response(const std::string& body) {
    const auto document = json_input::object(body);
    if (!document) return std::nullopt;
    const auto integer = [](const json_input::Json& object, const char* key)
        -> std::optional<int64_t> {
        if (!object.is_object()) return std::nullopt;
        const auto it = object.find(key);
        if (it == object.end() || !it->is_number_integer()) return std::nullopt;
        if (it->is_number_unsigned() &&
            it->get<uint64_t>() > uint64_t(std::numeric_limits<int64_t>::max()))
            return std::nullopt;
        return it->get<int64_t>();
    };
    const auto id = integer(*document, "match_id");
    if (!id || *id <= 0) return std::nullopt;
    const auto side = [&](const char* key, MatchDelta& out) {
        const auto it = document->find(key);
        if (it == document->end() || !it->is_object()) return false;
        const auto before = integer(*it, "elo_before");
        const auto after = integer(*it, "elo_after");
        const auto delta = integer(*it, "delta");
        if (!before || !after || !delta) return false;
        constexpr auto max = std::numeric_limits<int>::max();
        constexpr auto min = std::numeric_limits<int>::min();
        if (*before < 0 || *before > max || *after < 0 || *after > max ||
            *delta < min || *delta > max || *after - *before != *delta)
            return false;
        out = {static_cast<int>(*before), static_cast<int>(*after), static_cast<int>(*delta)};
        return true;
    };
    MatchResult result{};
    result.match_id = *id;
    if (!side("a", result.a) || !side("b", result.b)) return std::nullopt;
    return result;
}
} // namespace meta::client
