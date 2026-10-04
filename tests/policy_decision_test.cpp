#include "bot/policy_decision.h"
#include "bot/reward_replay.h"
#include "bot/reward_catalog.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
void require(bool ok){if(!ok)throw std::runtime_error("policy decision contract");}
int main() {
    std::istringstream official("r|R|@heuristic|||N|1|0|1\n");
    require(bot::read_reward_catalog(official).front().id=="r");
    for(const auto& text:{"# empty\n","r|R|@heuristic|||N|1|0|1\nr|Duplicate|@heuristic|||N|1|0|1\n","r|R|@heuristic|||N|1|0|1\ninvalid"}) {
        std::istringstream input(text);bool rejected=false;
        try{bot::read_reward_catalog(input);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected);
    }
    SimGame sim(42);const auto hash=sim.DiagnosticStateHashV2();
    int c=-7,r=-8,fallbackCalls=0;
    auto fallback=[&](const SimGame& s,int& x,int& y){++fallbackCalls;return bot::fallback_placement(s,x,y);};
    auto fail=[](const SimGame&,int&,int&){return false;};
    auto result=bot::choose_policy(sim,bot::heuristic_placement,fallback,false,c,r);
    require(result.source==bot::DecisionSource::primary && result.fault==bot::Fault::none && fallbackCalls==0);
    c=-7;r=-8;result=bot::choose_policy(sim,fail,fallback,false,c,r);
    require(!result.selected() && result.fault==bot::Fault::policy_failed && c==-7 && r==-8 && fallbackCalls==0);
    result=bot::choose_policy(sim,fail,fallback,true,c,r);
    require(result.source==bot::DecisionSource::fallback && result.fault==bot::Fault::policy_failed && fallbackCalls==1);
    bot::RunStatus status;status.reset(bot::Mode::reward);status.observe(result.fault);
    require(!status.reward_eligible());
    auto recovered=bot::choose_policy(sim,bot::heuristic_placement,fallback,true,c,r);status.observe(recovered.fault);
    require(recovered.source==bot::DecisionSource::primary && !status.reward_eligible());
    auto invalid=[](const SimGame&,int& x,int& y){x=-1;y=0;return true;};
    c=-7;r=-8;result=bot::choose_policy(sim,invalid,fallback,false,c,r);
    require(result.fault==bot::Fault::invalid_target && !result.selected() && c==-7 && r==-8);
    auto throwing=[](const SimGame&,int&,int&)->bool{throw std::runtime_error("fixture");};
    result=bot::choose_policy(sim,throwing,fail,true,c,r);
    require(result.fault==bot::Fault::policy_failed && !result.selected() && c==-7 && r==-8);
    require(sim.DiagnosticStateHashV2()==hash);
    // A finished board has no candidates; it must not invoke either callback.
    SimGame ended(42);while(!ended.IsGameOver()){ended.SubmitInput(INPUT_DROP);ended.Tick();}
    int calls=0;auto forbidden=[&](const SimGame&,int&,int&)->bool{++calls;throw std::runtime_error("unexpected");};
    result=bot::choose_policy(ended,forbidden,forbidden,true,c,r);
    require(result.source==bot::DecisionSource::no_legal && result.fault==bot::Fault::none && calls==0);
    bot::Opponent profile;profile.inputIntervalTicks=1;profile.thinkTicks=0;profile.minPieceTicks=1;
    require(bot::verify_result(42,profile,{0},fail)==bot::Verification::policy_unavailable);
    require(bot::verify_result(42,profile,{0},invalid)==bot::Verification::policy_unavailable);
    require(bot::verify_result(42,profile,{0},throwing)==bot::Verification::policy_unavailable);
    require(bot::verify_result(42,profile,{0},bot::heuristic_placement)==bot::Verification::not_victory);
    std::cout<<"primary/fallback provenance, fault latch, empty candidates and strict replay passed\n";
}
