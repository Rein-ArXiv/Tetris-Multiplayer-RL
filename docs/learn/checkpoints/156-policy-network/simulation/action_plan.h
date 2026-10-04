#pragma once
#include "simulation/action_space.h"
#include "simulation/input_mask.h"
#include "simulation/round.h"
#include <vector>

namespace study_actions {
struct Plan {
    study_round::Round result;
    std::vector<study_input::Mask> inputs;
};

// This route family rotates clockwise, translates, then drops. It is not BFS
// over every reachable placement. Each input consumes a real Round tick.
inline std::optional<Plan> plan(const study_round::Round& start, int action) {
    const auto target=decode(action);
    if(!target || start.finished() || !start.active())return std::nullopt;
    Plan candidate{start,{}};
    auto tick=[&](study_input::Mask mask) {
        const auto intent=*study_input::decode(mask);
        const auto result=candidate.result.tick(intent.horizontal,intent.clockwise,
                                                intent.soft_drop,intent.hard_drop);
        candidate.inputs.push_back(mask);
        return result;
    };
    const auto continued=[](study_round::Step step) {
        return step==study_round::Step::waiting || step==study_round::Step::changed;
    };
    const int turns=(target->quarter-start.quarter()+kOrientations)%kOrientations;
    for(int i=0;i<turns;++i) {
        const int expected=(candidate.result.quarter()+1)%kOrientations;
        if(!continued(tick(study_input::rotate)) || candidate.result.quarter()!=expected)
            return std::nullopt; // Blocked rotation or a lock before the intended drop.
    }
    // Rotation kicks may change the origin: derive translation from that result.
    while(candidate.result.active()->origin.column!=target->column) {
        const int before=candidate.result.active()->origin.column;
        const int direction=before<target->column?1:-1;
        if(!continued(tick(direction>0?study_input::right:study_input::left)) ||
           candidate.result.active()->origin.column!=before+direction)
            return std::nullopt;
    }
    const auto last=tick(study_input::drop);
    if(last!=study_round::Step::locked && last!=study_round::Step::game_over)
        return std::nullopt;
    return candidate;
}

inline std::vector<int> legal_actions(const study_round::Round& round) {
    std::vector<int> actions;
    for(int action=0;action<kCount;++action)
        if(plan(round,action))actions.push_back(action);
    return actions;
}
} // namespace study_actions
