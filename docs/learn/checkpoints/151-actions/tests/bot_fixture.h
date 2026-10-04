#pragma once
#include "simulation/bot_replay.h"
#include <stdexcept>
namespace study_bot_test {
inline void require(bool ok){if(!ok)throw std::runtime_error("bot reward contract");}
inline std::vector<std::uint8_t> proof(std::uint64_t seed,const study_bot::Opponent& p) {
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},seed);
    require(round.has_value());study_combat::Duel duel(*round,*round);
    std::vector<std::uint8_t> result;
    for(std::size_t i=0;i<1000;++i) {
        result.push_back(0);require(duel.tick({},study_bot::bot_intent(p,i)).has_value());
        if(duel.left().finished() || duel.right().finished()) {
            require(!duel.left().finished() && duel.right().finished());return result;
        }
    }
    throw std::runtime_error("fixture did not finish");
}
inline std::string hex(const std::vector<std::uint8_t>& input) {
    const char* digits="0123456789abcdef";std::string result;
    for(auto value:input){result+=digits[value>>4];result+=digits[value&15];}return result;
}
inline std::string key(unsigned char byte){return std::string(30,'a')+"0123456789abcdef"[byte>>4]+"0123456789abcdef"[byte&15];}
}
