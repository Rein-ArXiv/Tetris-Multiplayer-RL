#pragma once
#include "../third_party/json.hpp"
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace meta::json_input {
using Json = nlohmann::json;

// Validate the entire document, including UTF-8, delimiters and duplicate keys.
// Depth and byte limits apply before any endpoint extracts a field. Throwing in
// the callback aborts parsing; merely filtering a deep subtree would still parse it.
inline std::optional<Json> object(const std::string &body) {
    if (body.size() > 64 * 1024)
        return std::nullopt;
    try {
        std::vector<std::set<std::string>> keys;
        auto callback = [&](int depth, Json::parse_event_t event, Json &value) {
            if (depth > 16)
                throw std::runtime_error("JSON nesting limit");
            if (event == Json::parse_event_t::object_start)
                keys.emplace_back();
            if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate JSON key");
            if (event == Json::parse_event_t::object_end)
                keys.pop_back();
            return true;
        };
        auto parsed = Json::parse(body, callback);
        if (parsed.is_object())
            return parsed;
    } catch (const std::exception &) {
        // Do not log parser diagnostics: they can contain bearer credentials.
    }
    return std::nullopt;
}
inline std::string string(const std::string &body, const char *key) {
    const auto parsed = object(body);
    if (!parsed)
        return {};
    const auto it = parsed->find(key);
    return it != parsed->end() && it->is_string() ? it->get<std::string>() : "";
}
inline std::optional<int64_t> integer(const std::string &body, const char *key) {
    const auto parsed = object(body);
    if (!parsed)
        return std::nullopt;
    const auto it = parsed->find(key);
    if (it == parsed->end() || !it->is_number_integer())
        return std::nullopt;
    if (it->is_number_unsigned() && it->get<uint64_t>() > uint64_t(INT64_MAX))
        return std::nullopt;
    return it->get<int64_t>();
}
inline std::optional<bool> boolean(const std::string &body, const char *key) {
    const auto parsed = object(body);
    if (!parsed)
        return std::nullopt;
    const auto it = parsed->find(key);
    if (it == parsed->end() || !it->is_boolean())
        return std::nullopt;
    return it->get<bool>();
}
} // namespace meta::json_input
