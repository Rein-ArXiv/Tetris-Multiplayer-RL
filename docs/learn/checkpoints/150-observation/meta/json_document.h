#pragma once
#include "json.hpp"
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace study_meta {
using Json = nlohmann::json;
inline constexpr std::size_t kJsonBodyBytes = 1024;

// Parse one complete flat object. Field names/types are checked by the caller.
inline std::optional<Json> object(const std::string& body, std::size_t fields) {
    if (body.size() > kJsonBodyBytes || body.find('\0') != std::string::npos)
        return std::nullopt;
    try {
        std::set<std::string> keys;
        auto callback = [&](int depth, Json::parse_event_t event, Json& value) {
            if (event == Json::parse_event_t::array_start ||
                (event == Json::parse_event_t::object_start && depth != 0))
                throw std::runtime_error("flat object required");
            if (event == Json::parse_event_t::key &&
                !keys.insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate key");
            return true;
        };
        auto result = Json::parse(body, callback);
        if (result.is_object() && result.size() == fields)
            return result;
    } catch (const std::exception&) {
        // Parser diagnostics may contain the request body; do not log them.
    }
    return std::nullopt;
}
} // namespace study_meta
