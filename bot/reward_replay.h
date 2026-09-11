#pragma once
#include "controller.h"
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

template<class Picker>
bool verify_victory(uint64_t seed, const Opponent& opponent,
                    const std::vector<uint8_t>& inputs, Picker picker) {
    if(inputs.empty() || inputs.size()>kMaxRewardTicks) return false;
    SimGame human(seed), enemy(seed);
    Controller controller;
    controller.reset(opponent.inputIntervalTicks,opponent.thinkTicks,opponent.minPieceTicks);
    int humanAttack=0, enemyAttack=0;
    const auto start=std::chrono::steady_clock::now();
    for(size_t i=0;i<inputs.size();++i) {
        if(inputs[i]&~uint8_t(31))return false;
        if(i%120==0 && std::chrono::steady_clock::now()-start>std::chrono::seconds(5))return false;
        auto botInput=controller.next(enemy,picker);
        human.SubmitInput(inputs[i]); enemy.SubmitInput(botInput);
        human.Tick(); enemy.Tick();
        exchange_garbage(human,enemy,humanAttack,enemyAttack);
        if(human.IsGameOver() || enemy.IsGameOver())
            return i+1==inputs.size() && enemy.IsGameOver() && !human.IsGameOver();
    }
    return false;
}
inline bool decode_inputs(const std::string& hex, std::vector<uint8_t>& out) {
    if(hex.empty() || hex.size()%2 || hex.size()>kMaxRewardTicks*2) return false;
    auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    out.clear(); out.reserve(hex.size()/2);
    for(size_t i=0;i<hex.size();i+=2) {
        int a=digit(hex[i]),b=digit(hex[i+1]);
        if(a<0||b<0||((a*16+b)&~31))return false;
        out.push_back(uint8_t(a*16+b));
    }
    return true;
}
}
