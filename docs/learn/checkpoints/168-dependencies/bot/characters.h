#pragma once
#include "bot/opponent_profile.h"
#include <istream>
#include <stdexcept>
#include <unordered_set>

namespace study_characters {
struct Appearance {
    std::string name, icon, portrait, difficulty;
};
struct Behavior {
    std::string model;
    bot::Pacing pacing;
};
struct Character {
    std::string id;
    Appearance appearance;
    Behavior behavior;
};
// Build a complete candidate first. Assignment to a live catalog happens at the caller.
class Catalog {
public:
    static Catalog read(std::istream& input) {
        Catalog candidate;
        std::unordered_set<std::string> ids;
        std::string line;std::size_t number=0;
        while(std::getline(input,line)) {
            ++number;
            auto parsed=bot::parse_opponent_profile(line);
            if(!parsed.error.empty())throw std::invalid_argument("profile line "+std::to_string(number)+": "+parsed.error);
            if(!parsed.entry)continue;
            const auto& e=*parsed.entry;
            if(!ids.insert(e.id).second)throw std::invalid_argument("duplicate character identity");
            candidate.entries_.push_back({e.id,{e.name,e.iconPath,e.portraitPath,e.difficulty},
                {e.path,{e.inputIntervalTicks,e.thinkTicks,e.minPieceTicks}}});
        }
        if(input.bad() || (input.fail() && !input.eof()))throw std::runtime_error("profile read failed");
        if(candidate.entries_.empty())throw std::invalid_argument("empty character catalog");
        return candidate;
    }
    // A match owns a value snapshot; catalog replacement cannot change its opponent.
    Character select(const std::string& id) const {
        for(const auto& entry:entries_)if(entry.id==id)return entry;
        throw std::out_of_range("unknown character identity");
    }
    const std::vector<Character>& entries() const noexcept {return entries_;}
private:
    std::vector<Character> entries_;
};
}
