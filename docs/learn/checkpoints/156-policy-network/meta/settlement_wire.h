#ifndef STUDY_META_SETTLEMENT_WIRE_H
#define STUDY_META_SETTLEMENT_WIRE_H

#include "meta/wire.h"

#include <cstdint>
#include <optional>
#include <string>

namespace study_meta {

// The original match identity and saved awards, not the current balances.
struct Settlement {
    study_net::Receipt match;
    std::uint32_t bp_a = 0;
    std::uint32_t bp_b = 0;
    std::uint32_t policy = 0;
};

namespace settlement_detail {

inline bool awards_ok(std::uint64_t policy, std::uint64_t bp_a, std::uint64_t bp_b) {
    if (bp_a > 10 || bp_b > 10) {
        return false;
    }
    if (policy == 0) {
        return bp_a == 0 && bp_b == 0;
    }
    return (bp_a == 0 && bp_b == 0) || (bp_a == 10 && bp_b == 3) ||
           (bp_a == 3 && bp_b == 10);
}

}  // namespace settlement_detail

inline Json settlement_json(const Settlement& settlement) {
    Json json = receipt_json(settlement.match);
    json["bp_a"] = settlement.bp_a;
    json["bp_b"] = settlement.bp_b;
    json["policy"] = settlement.policy;
    return json;
}

inline std::optional<Settlement> parse_settlement(const std::string& body) {
    const std::optional<Json> parsed = object(body, 7);
    if (!parsed) {
        return std::nullopt;
    }
    const Json& json = *parsed;

    const std::optional<std::uint64_t> key = number(json, "key");
    const std::optional<std::uint64_t> row = number(json, "row");
    const std::optional<std::uint64_t> player_a = number(json, "player_a");
    const std::optional<std::uint64_t> player_b = number(json, "player_b");
    const std::optional<std::uint64_t> bp_a = number(json, "bp_a");
    const std::optional<std::uint64_t> bp_b = number(json, "bp_b");
    const std::optional<std::uint64_t> policy = number(json, "policy");
    if (!key || !row || !player_a || !player_b || !bp_a || !bp_b || !policy) {
        return std::nullopt;
    }

    // Identity and policy/award invariants, all still in std::uint64_t.
    if (*key == 0 || *row == 0 || *player_a == 0 || *player_b == 0) {
        return std::nullopt;
    }
    if (*player_a == *player_b) {
        return std::nullopt;
    }
    if (*policy > 1) {
        return std::nullopt;
    }
    if (!settlement_detail::awards_ok(*policy, *bp_a, *bp_b)) {
        return std::nullopt;
    }

    return Settlement{{*key, *row, *player_a, *player_b},
                      static_cast<std::uint32_t>(*bp_a),
                      static_cast<std::uint32_t>(*bp_b),
                      static_cast<std::uint32_t>(*policy)};
}

}  // namespace study_meta

#endif  // STUDY_META_SETTLEMENT_WIRE_H
