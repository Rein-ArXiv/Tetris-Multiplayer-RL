#pragma once
#include "net/match_submission.h"
#include "meta/json_fields.h"
#include <limits>
namespace study_meta {
inline Json record_json(const study_net::MatchRecord& r) {
    return {{"key",r.key},{"round",r.round},{"player_a",r.player_a},{"player_b",r.player_b},
            {"ticks",r.ticks},{"score_a",r.score_a},{"score_b",r.score_b},
            {"lines_a",r.lines_a},{"lines_b",r.lines_b},{"winner",static_cast<unsigned>(r.winner)}};
}
inline std::optional<study_net::MatchRecord> parse_record(const std::string& body) {
    const auto j=object(body,10);if(!j)return std::nullopt;
    const auto key=number(*j,"key"),round=number(*j,"round"),a=number(*j,"player_a"),b=number(*j,"player_b");
    const auto ticks=number(*j,"ticks"),sa=number(*j,"score_a"),sb=number(*j,"score_b");
    const auto la=number(*j,"lines_a"),lb=number(*j,"lines_b"),winner=number(*j,"winner");
    if(!key || !round || !a || !b || !ticks || !sa || !sb || !la || !lb || !winner ||
       *la>std::numeric_limits<std::uint32_t>::max() || *lb>std::numeric_limits<std::uint32_t>::max() || *winner>2)
        return std::nullopt;
    return study_net::MatchRecord{*key,*round,*a,*b,*ticks,*sa,*sb,
        static_cast<std::uint32_t>(*la),static_cast<std::uint32_t>(*lb),
        static_cast<study_net::MatchRecord::MatchWinner>(*winner)};
}
inline Json receipt_json(const study_net::Receipt& r) {
    return {{"key",r.key},{"row",r.row},{"player_a",r.player_a},{"player_b",r.player_b}};
}
inline std::optional<study_net::Receipt> parse_receipt(const std::string& body) {
    const auto j=object(body,4);if(!j)return std::nullopt;
    const auto key=number(*j,"key"),row=number(*j,"row"),a=number(*j,"player_a"),b=number(*j,"player_b");
    if(!key || !row || !a || !b || !*key || !*row || !*a || !*b || *a==*b)return std::nullopt;
    return study_net::Receipt{*key,*row,*a,*b};
}
} // namespace study_meta
