#include "../bot/reward_replay.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>

static bool pick(const SimGame& s,int& c,int& r) {
    return bot::heuristic_placement(s,c,r) || bot::fallback_placement(s,c,r);
}
int main(int argc,char** argv) {
    const uint64_t seed=argc>1?std::stoull(argv[1]):42;
    bot::Opponent enemyProfile;
    enemyProfile.inputIntervalTicks=1;enemyProfile.thinkTicks=0;enemyProfile.minPieceTicks=1;
    SimGame human(seed),enemy(seed);
    bot::Controller player,opponent;
    player.reset(8,24,90);opponent.reset(1,0,1);
    std::vector<uint8_t> inputs;
    int ha=0,ea=0;
    while(!human.IsGameOver() && !enemy.IsGameOver() && inputs.size()<bot::kMaxRewardTicks) {
        auto h=player.next(human,pick),e=opponent.next(enemy,pick);
        inputs.push_back(h);human.SubmitInput(h);enemy.SubmitInput(e);
        human.Tick();enemy.Tick();bot::exchange_garbage(human,enemy,ha,ea);
    }
    if(!bot::verify_victory(seed,enemyProfile,inputs,pick)) {
        std::fprintf(stderr,"fixture did not win (%zu ticks, human %d/%d bot %d/%d attack %d/%d)\n",inputs.size(),human.IsGameOver(),human.score,enemy.IsGameOver(),enemy.score,ha,ea);return 1;
    }
    if(argc>1) {
        const char* hex="0123456789abcdef";
        for(auto i:inputs)std::cout<<hex[i>>4]<<hex[i&15];
        std::cout<<'\n';return 0;
    }
    inputs.pop_back();
    if(bot::verify_victory(seed,enemyProfile,inputs,pick))return 2;
    inputs.assign(60,0);
    if(bot::verify_victory(seed,enemyProfile,inputs,pick))return 3;
    inputs.assign(1,255);
    if(bot::verify_victory(seed,enemyProfile,inputs,pick))return 4;
    std::vector<uint8_t> decoded;
    if(!bot::decode_inputs("001f10",decoded) || decoded.size()!=3 || decoded[1]!=31)return 5;
    for(auto bad:{"","0","20","ff","GG","00 0"})if(bot::decode_inputs(bad,decoded))return 6;
    return 0;
}
