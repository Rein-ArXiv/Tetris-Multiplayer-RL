#pragma once
#include "simulation/duel.h"
#include "net/match_submission.h"
#include <limits>
#include <optional>
// Trusted local fixture; no externally supplied scores or win claims.
inline std::optional<study_net::MatchRecord> sample_match(std::uint64_t key) {
    const auto board=study_round::Round::create_seeded(study_grid::Grid{},77);if(!board)return std::nullopt;
    study_combat::Duel game(*board,*board);
    study_input::Intent left{},right{};left.hard_drop=true;
    std::uint64_t ticks=0;
    while(!game.left().finished() && !game.right().finished()) {
        if(ticks>=1000 || !game.tick(left,right))return std::nullopt;
        ++ticks;
    }
    const auto& a=game.left();const auto& b=game.right();
    if(a.total_lines()>std::numeric_limits<std::uint32_t>::max() ||
       b.total_lines()>std::numeric_limits<std::uint32_t>::max())return std::nullopt;
    const auto winner=a.finished()==b.finished() ? study_net::MatchRecord::draw
        : a.finished() ? study_net::MatchRecord::b : study_net::MatchRecord::a;
    return study_net::MatchRecord{key,1,101,202,ticks,a.score(),b.score(),
        static_cast<std::uint32_t>(a.total_lines()),static_cast<std::uint32_t>(b.total_lines()),winner};
}
