#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "meta/json_fields.h"

namespace study_meta {

// A leaderboard carries at most three rows.
constexpr std::size_t kRankingLimit = 3;

// Largest value the signed 32-bit rp field may hold.
constexpr std::int64_t kMaxRp = 2147483647;

struct RankRow {
    std::uint64_t player_id = 0;
    int rp = 0;
};

using Ranking = std::vector<RankRow>;

// Validates size, id positivity, uniqueness, rp range and the ordering
// contract: rp non-increasing, then player_id ascending for equal rp.
inline bool valid_ranking(const Ranking& rows) {
    if (rows.size() > kRankingLimit) {
        return false;
    }
    std::set<std::uint64_t> ids;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const RankRow& row = rows[i];
        if (row.player_id == 0 || row.rp < 0 || static_cast<std::int64_t>(row.rp) > kMaxRp) {
            return false;
        }
        if (!ids.insert(row.player_id).second) {
            return false;
        }
        if (i > 0) {
            const RankRow& prev = rows[i - 1];
            if (prev.rp < row.rp) {
                return false;
            }
            if (prev.rp == row.rp && prev.player_id >= row.player_id) {
                return false;
            }
        }
    }
    return true;
}

// Serializes a valid ranking as the exact array-of-objects wire shape.
inline Json ranking_json(const Ranking& rows) {
    if (!valid_ranking(rows)) {
        throw std::invalid_argument("study_meta::ranking_json: invalid ranking");
    }
    Json root = Json::array();
    for (const RankRow& row : rows) {
        root.push_back(Json{{"player_id", row.player_id}, {"rp", row.rp}});
    }
    return root;
}

// Parses and validates a request body. Returns nullopt for any malformed or
// contract-violating input; an empty root array yields a successful empty
// ranking. The body is never logged and data is never coerced or reordered.
inline std::optional<Ranking> parse_ranking(const std::string& body) {
    if (body.size() > kJsonBodyBytes) {
        return std::nullopt;
    }
    if (body.find('\0') != std::string::npos) {
        return std::nullopt;
    }

    // Per-row key set: duplicates are rejected on the key event, before the
    // DOM line would overwrite a previous value.
    std::set<std::string> seen_keys;
    std::size_t object_count = 0;

    Json::parser_callback_t callback =
        [&seen_keys, &object_count](int depth, Json::parse_event_t event, Json& parsed) -> bool {
            switch (event) {
                case Json::parse_event_t::array_start:
                    if (depth != 0) {
                        throw std::invalid_argument("parse_ranking: nested array");
                    }
                    return true;
                case Json::parse_event_t::object_start:
                    if (depth != 1) {
                        throw std::invalid_argument("parse_ranking: object outside row slot");
                    }
                    if (++object_count > kRankingLimit) {
                        throw std::invalid_argument("parse_ranking: too many rows");
                    }
                    seen_keys.clear();
                    return true;
                case Json::parse_event_t::key:
                    if (!seen_keys.insert(parsed.get<std::string>()).second) {
                        throw std::invalid_argument("parse_ranking: duplicate key");
                    }
                    return true;
                default:
                    return true;
            }
        };

    try {
        const Json root = Json::parse(body, callback);

        if (!root.is_array() || root.size() > kRankingLimit) {
            return std::nullopt;
        }

        Ranking rows;
        rows.reserve(root.size());
        std::set<std::uint64_t> ids;
        for (const Json& element : root) {
            if (!element.is_object() || element.size() != 2) {
                return std::nullopt;
            }
            const std::optional<std::uint64_t> id = number(element, "player_id");
            const std::optional<std::uint64_t> rp = number(element, "rp");
            if (!id || !rp) {
                return std::nullopt;
            }
            if (*id == 0) {
                return std::nullopt;
            }
            if (*rp > static_cast<std::uint64_t>(kMaxRp)) {
                return std::nullopt;
            }
            if (!ids.insert(*id).second) {
                return std::nullopt;
            }
            rows.push_back(RankRow{*id, static_cast<int>(*rp)});
        }

        if (!valid_ranking(rows)) {
            return std::nullopt;
        }
        return rows;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace study_meta