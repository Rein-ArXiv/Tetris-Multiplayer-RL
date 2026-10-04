#pragma once
#include "run_status.h"
#include "placement.h"
#include "../src/sim_game.h"
#include <algorithm>
#include <exception>

namespace bot {
enum class DecisionSource { primary, fallback, no_legal, unavailable };
struct Decision {
    DecisionSource source=DecisionSource::unavailable;
    Fault fault=Fault::none;
    bool selected() const noexcept {
        return source==DecisionSource::primary || source==DecisionSource::fallback;
    }
};
// Infer only when a legal target exists. An empty set is a rule state, not a
// model fault. On faults the caller explicitly chooses whether practice may continue.
template<class Primary, class Fallback>
Decision choose_policy(const SimGame& sim,Primary primary,Fallback fallback,
                       bool allowFallback,int& column,int& rotation) {
    const auto legal=sim.LegalPlacements();
    if(legal.empty())return {DecisionSource::no_legal,Fault::none};
    const auto valid=[&](int c,int r) {
        return std::any_of(legal.begin(),legal.end(),[&](const auto& p){return p.col==c && p.rot==r;});
    };
    int c=-1,r=-1;bool ok=false;
    try {ok=primary(sim,c,r);}catch(const std::exception&) {}
    const auto fault=ok ? Fault::invalid_target : Fault::policy_failed;
    if(ok && valid(c,r)) {column=c;rotation=r;return {DecisionSource::primary,Fault::none};}
    if(allowFallback) {
        c=-1;r=-1;ok=false;
        try {ok=fallback(sim,c,r);}catch(const std::exception&) {}
        if(ok && valid(c,r)) {column=c;rotation=r;return {DecisionSource::fallback,fault};}
    }
    return {DecisionSource::unavailable,fault};
}
}
