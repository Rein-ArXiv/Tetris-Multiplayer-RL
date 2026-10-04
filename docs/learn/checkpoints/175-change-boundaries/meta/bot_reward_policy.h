#pragma once
#include <cstdint>
#include <algorithm>
#include <stdexcept>

namespace study_meta {

struct BotRewardPolicy {
    int per_win;
    int daily_cap;
};

// Settles one bot victory payout. `already_earned` counts durable awarded
// receipts accumulated in the current server UTC day; it is NOT the player's
// current spendable balance. Only a trusted, verified victory may invoke this
// settlement path; this helper alone does not prove victory and must not run
// on unverified input. A zero result is a valid recorded settlement, not an
// error.
inline int bot_award(BotRewardPolicy policy, std::int64_t already_earned) {
    if (policy.per_win < 0 || policy.daily_cap < 0 || already_earned < 0) {
        throw std::invalid_argument("bot_award: negative policy or earned value");
    }
    // Compare before subtracting so no signed overflow occurs, even at INT64_MAX.
    const std::int64_t cap = static_cast<std::int64_t>(policy.daily_cap);
    const std::int64_t remaining =
        (already_earned >= cap) ? 0 : (cap - already_earned);
    const std::int64_t award =
        std::min<std::int64_t>(static_cast<std::int64_t>(policy.per_win), remaining);
    return static_cast<int>(award);
}

}  // namespace study_meta
