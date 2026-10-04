#ifndef STUDY_META_REVIEW_POLICY_H
#define STUDY_META_REVIEW_POLICY_H

// Factual review observations only.
// These flags identify patterns that require human/automated review.
// They are observations, NOT proof of cheating and NOT proof of human identity.

#include <cstdint>
#include <algorithm>
#include <stdexcept>

namespace study_meta {

struct ReviewCounts {
    std::uint64_t games;
    std::uint64_t lower_wins;
    std::uint64_t upper_wins;
    std::uint64_t short_games;
};

struct ReviewPolicy {
    std::uint64_t repeat_matches;
    std::uint64_t one_sided_wins;
    std::uint64_t short_matches;
};

enum ReviewFlag : unsigned {
    repeated_pair = 1,
    one_sided = 2,
    short_results = 4
};

// Returns a bitmask of ReviewFlag observations. Counts are read only and never mutated.
inline unsigned review_flags(ReviewCounts c, ReviewPolicy p) {
    if (p.repeat_matches == 0 || p.one_sided_wins == 0 || p.short_matches == 0) {
        throw std::invalid_argument("review policy thresholds must be non-zero");
    }
    // Consistency: total wins must not exceed games (checked without overflow),
    // and lower_wins must not exceed games - upper_wins.
    if (c.upper_wins > c.games || c.lower_wins > c.games - c.upper_wins) {
        throw std::invalid_argument("inconsistent counters: wins exceed games");
    }
    if (c.short_games > c.games) {
        throw std::invalid_argument("inconsistent counters: short_games exceed games");
    }

    unsigned flags = 0;

    if (c.games >= p.repeat_matches) {
        flags |= repeated_pair;
    }

    const std::uint64_t hi = std::max(c.lower_wins, c.upper_wins);
    const std::uint64_t lo = std::min(c.lower_wins, c.upper_wins);
    if (hi >= p.one_sided_wins && lo == 0) {
        flags |= one_sided;
    }

    if (c.short_games >= p.short_matches) {
        flags |= short_results;
    }

    return flags;
}

// Factual Korean labels for review observations; unknown flags get a neutral fallback.
inline const char* review_label(ReviewFlag f) {
    switch (f) {
        case repeated_pair: return "반복 상대";
        case one_sided:     return "한쪽으로 치우친 승패";
        case short_results: return "짧은 경기 다수";
        default:            return "알 수 없는 관찰 항목";
    }
}

} // namespace study_meta

#endif // STUDY_META_REVIEW_POLICY_H
