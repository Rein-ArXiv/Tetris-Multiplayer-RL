#pragma once
#include "meta/review_policy.h"
#include <array>
#include <map>
#include <vector>
#include <cstddef>
#include <stdexcept>
namespace study_meta {
// Fixed checkpoint safety ceiling; a deployment supplies a smaller sample policy.
inline constexpr std::size_t kMaxReviewRows=100000;
struct ReviewRow {
    std::uint64_t row,player_a,player_b,winner,ticks; // winner==0 means draw
};
struct ReviewSample {
    std::vector<ReviewRow> rows; // strictly descending durable row ID
    std::size_t requested=0;
    bool has_older=false;
};
struct PairReview {
    std::array<std::uint64_t,2> players;
    ReviewCounts counts{};
    unsigned flags=0;
};
struct ReviewReport {
    ReviewPolicy policy;
    std::uint64_t short_tick_limit;
    std::size_t rows_read,requested;
    std::uint64_t newest=0,oldest=0;
    bool has_older;
    std::vector<PairReview> pairs;
};
inline ReviewReport review_sample(const ReviewSample& sample,ReviewPolicy policy,
                                  std::uint64_t short_tick_limit) {
    (void)review_flags({},policy); // Validate thresholds even for an empty sample.
    if(!sample.requested || sample.requested>kMaxReviewRows || sample.rows.size()>sample.requested ||
       !short_tick_limit || (sample.has_older && sample.rows.size()!=sample.requested))
        throw std::invalid_argument("invalid review sample policy");
    ReviewReport report{policy,short_tick_limit,sample.rows.size(),sample.requested,0,0,sample.has_older,{}};
    std::map<std::array<std::uint64_t,2>,ReviewCounts> pairs;
    std::uint64_t previous=0;
    for(const auto& row:sample.rows) {
        if(!row.row || (previous && row.row>=previous) || !row.player_a || !row.player_b ||
           row.player_a==row.player_b || !row.ticks ||
           (row.winner && row.winner!=row.player_a && row.winner!=row.player_b))
            throw std::invalid_argument("invalid authoritative review row");
        previous=row.row;
        const std::array<std::uint64_t,2> pair{std::min(row.player_a,row.player_b),std::max(row.player_a,row.player_b)};
        auto& count=pairs[pair];++count.games;
        if(row.winner==pair[0])++count.lower_wins;
        if(row.winner==pair[1])++count.upper_wins;
        if(row.ticks<=short_tick_limit)++count.short_games;
    }
    for(const auto& pair:pairs)report.pairs.push_back({pair.first,pair.second,review_flags(pair.second,policy)});
    if(!sample.rows.empty()){report.newest=sample.rows.front().row;report.oldest=sample.rows.back().row;}
    return report;
}
} // namespace study_meta
