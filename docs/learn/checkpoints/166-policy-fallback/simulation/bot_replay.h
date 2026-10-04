#pragma once
#include "simulation/duel.h"
#include "simulation/input_mask.h"
#include <string>
#include <vector>
namespace study_bot {
// A deliberately small official study opponent. Identity includes its revision.
// A server catalog supplies this value; the claim supplies only human inputs.
struct Opponent { std::string id; std::uint32_t revision=1,drop_every=1; };
inline bool valid(const Opponent& p) {return !p.id.empty() && p.revision==1 && p.drop_every>0;}
inline bool decode(const std::string& hex,std::size_t limit,std::vector<std::uint8_t>& out) {
    if(hex.empty() || hex.size()%2 || hex.size()/2>limit)return false;
    auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    std::vector<std::uint8_t> next;next.reserve(hex.size()/2);
    for(std::size_t i=0;i<hex.size();i+=2) {
        const int a=digit(hex[i]),b=digit(hex[i+1]);
        if(a<0 || b<0 || !study_input::valid(a*16+b))return false;
        next.push_back(static_cast<std::uint8_t>(a*16+b));
    }
    out=std::move(next);return true;
}
// This policy has no inference: every drop_every-th tick requests a hard drop.
// More capable policies may inspect the bot board, but must be reproduced here.
inline study_input::Intent bot_intent(const Opponent& p,std::size_t tick) {
    study_input::Intent result{};result.hard_drop=(tick%p.drop_every)==p.drop_every-1;return result;
}
inline bool verify(std::uint64_t seed,const Opponent& p,
                   const std::vector<std::uint8_t>& inputs,std::size_t max_ticks) {
    if(!valid(p) || inputs.empty() || inputs.size()>max_ticks)return false;
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},seed);
    if(!round)return false;
    study_combat::Duel duel(*round,*round);
    for(std::size_t i=0;i<inputs.size();++i) {
        const auto human=study_input::decode(inputs[i]);
        if(!human || !duel.tick(*human,bot_intent(p,i)))return false;
        if(duel.left().finished() || duel.right().finished())
            return i+1==inputs.size() && !duel.left().finished() && duel.right().finished();
    }
    return false;
}
} // namespace study_bot
