#pragma once
#include "controller.h"
#include "policy_decision.h"
#include "../core/input.h"
#include "opponents.h"
#include <chrono>
#include <string>
#include <vector>

namespace bot {
constexpr size_t kMaxRewardTicks = 30000; // 500 seconds at 60Hz; fits the 64KiB JSON limit.

// Shared stepping order: human input, bot input, both gravity ticks, then garbage.
inline void exchange_garbage(SimGame& human, SimGame& enemy, int& humanAttack, int& enemyAttack) {
    const int a=human.AttackLinesSent(), b=enemy.AttackLinesSent();
    if(a>humanAttack) enemy.AddPendingGarbage(a-humanAttack);
    if(b>enemyAttack) human.AddPendingGarbage(b-enemyAttack);
    humanAttack=a; enemyAttack=b;
}

enum class Verification { victory, not_victory, policy_unavailable };

template<class Picker>
Verification verify_result(uint64_t seed, const Opponent& opponent,
                           const std::vector<uint8_t>& inputs, Picker picker) {
    if(inputs.empty() || inputs.size()>kMaxRewardTicks) return Verification::not_victory;
    SimGame human(seed), enemy(seed);
    Controller controller;
    controller.reset(opponent.inputIntervalTicks,opponent.thinkTicks,opponent.minPieceTicks);
    int humanAttack=0, enemyAttack=0;
    RunStatus status;status.reset(Mode::reward);
    const auto strictPicker=[&](const SimGame& sim,int& col,int& rot) {
        const auto decision=choose_policy(sim,picker,fallback_placement,false,col,rot);
        status.observe(decision.fault);
        return decision.selected();
    };
    const auto start=std::chrono::steady_clock::now();
    for(size_t i=0;i<inputs.size();++i) {
        if(!isValidInputMask(inputs[i]))return Verification::not_victory;
        if(i%120==0 && std::chrono::steady_clock::now()-start>std::chrono::seconds(5))return Verification::not_victory;
        const auto botInput=controller.next(enemy,strictPicker);
        if(!status.reward_eligible())return Verification::policy_unavailable;
        // blocked/target_lost are reproducible route outcomes; replanning is normal.
        human.SubmitInput(inputs[i]); enemy.SubmitInput(botInput);
        human.Tick(); enemy.Tick();
        exchange_garbage(human,enemy,humanAttack,enemyAttack);
        if(human.IsGameOver() || enemy.IsGameOver())
            return i+1==inputs.size() && enemy.IsGameOver() && !human.IsGameOver()
                ? Verification::victory : Verification::not_victory;
    }
    return Verification::not_victory;
}
template<class Picker>
bool verify_victory(uint64_t seed,const Opponent& opponent,
                    const std::vector<uint8_t>& inputs,Picker picker) {
    return verify_result(seed,opponent,inputs,picker)==Verification::victory;
}
inline bool decode_inputs(const std::string& hex, std::vector<uint8_t>& out) {
    if(hex.empty() || hex.size()%2 || hex.size()>kMaxRewardTicks*2) return false;
    auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    out.clear(); out.reserve(hex.size()/2);
    for(size_t i=0;i<hex.size();i+=2) {
        int a=digit(hex[i]),b=digit(hex[i+1]);
        if(a<0||b<0||!isValidInputMask(static_cast<uint64_t>(a*16+b)))return false;
        out.push_back(uint8_t(a*16+b));
    }
    return true;
}
}
