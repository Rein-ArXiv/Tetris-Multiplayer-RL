#pragma once
#include "opponent_profile.h"
#include <istream>
#include <stdexcept>
#include <unordered_set>

namespace bot {
// Rewards admit only explicit official profiles. Local auto-discovery and the
// default practice partner are not an authorization source for this catalog.
inline std::vector<Opponent> read_reward_catalog(std::istream& input) {
    std::vector<Opponent> result;std::unordered_set<std::string> ids;
    std::string line;std::size_t number=0;
    while(std::getline(input,line)) {
        ++number;auto parsed=parse_opponent_profile(line);
        if(!parsed.error.empty())throw std::invalid_argument("reward profile line "+std::to_string(number)+": "+parsed.error);
        if(!parsed.entry)continue;
        if(!ids.insert(parsed.entry->id).second)throw std::invalid_argument("duplicate reward opponent");
        result.push_back(std::move(*parsed.entry));
    }
    if(input.bad() || (input.fail() && !input.eof()))throw std::runtime_error("reward catalog read failed");
    if(result.empty())throw std::invalid_argument("empty reward catalog");
    return result;
}
}
