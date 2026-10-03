#pragma once
#include "net/match_submission.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace study_meta {
inline constexpr int kProgressLimit = 2147483647;
struct Career { int rp=0; int xp=0; };
struct ProgressAward { int before=0; int after=0; int xp=0; int policy=0; };
struct ProgressPair { ProgressAward a; ProgressAward b; };
inline int rating_k(int rp) { return rp<300 ? 32 : (rp<600 ? 24 : 16); }
inline double expected_score(int a, int b) {
    return 1.0/(1.0+std::pow(10.0,(static_cast<double>(b)-static_cast<double>(a))/400.0));
}
inline int next_rating(int before, double score, double expected) {
    const auto delta=static_cast<int>(std::round(rating_k(before)*(score-expected)));
    const std::int64_t sum=static_cast<std::int64_t>(before)+delta;
    return static_cast<int>(std::clamp<std::int64_t>(sum,0,kProgressLimit));
}
inline ProgressPair progress_for(int a, int b, std::uint8_t winner) {
    if(a<0 || b<0)throw std::invalid_argument("negative rating");
    if(winner==study_net::MatchRecord::draw)return {{a,a,0,1},{b,b,0,1}};
    if(winner!=study_net::MatchRecord::a && winner!=study_net::MatchRecord::b)
        throw std::invalid_argument("invalid winner");
    const bool a_won=winner==study_net::MatchRecord::a;
    return {{a,next_rating(a,a_won?1.0:0.0,expected_score(a,b)),a_won?100:50,1},
            {b,next_rating(b,a_won?0.0:1.0,expected_score(b,a)),a_won?50:100,1}};
}
inline int add_xp(int before, int amount) {
    if(before<0 || amount<0 || amount>100 || before>kProgressLimit-amount)
        throw std::overflow_error("XP range exceeded");
    return before+amount;
}
} // namespace study_meta
