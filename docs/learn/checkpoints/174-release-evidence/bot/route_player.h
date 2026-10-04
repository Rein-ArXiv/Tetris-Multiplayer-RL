#pragma once
#include "simulation/action_plan.h"
#include "simulation/state_hash.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace study_route {
enum class Status { idle, ready, awaiting_feedback, complete, rejected, stale };

inline bool same(const study_hash::RoundBytes& expected,const study_round::Round& live) noexcept {
    const auto actual=study_hash::state_bytes(live);
    return expected.ok() && actual.ok() && expected.size()==actual.size() &&
           std::equal(expected.data(),expected.data()+expected.size(),actual.data());
}

// One owner calls start -> issue -> one real tick -> observe, in that order.
// A route predicts consecutive ticks; external waits or garbage can invalidate it.
class Player {
public:
    Status status() const noexcept {return status_;}
    std::size_t confirmed() const noexcept {return cursor_;}
    void cancel() noexcept {inputs_.clear();states_.clear();cursor_=0;status_=Status::idle;}

    bool start(const study_round::Round& live,int action) {
        cancel();
        const auto route=study_actions::plan(live,action);
        if(!route) {status_=Status::rejected;return false;}
        if(route->inputs.empty())throw std::logic_error("planner returned an empty trace");
        auto predicted=live;
        std::vector<study_hash::RoundBytes> states;
        states.reserve(route->inputs.size()+1);
        states.push_back(study_hash::state_bytes(predicted));
        for(const auto mask:route->inputs) {
            const auto input=*study_input::decode(mask);
            const auto step=predicted.tick(input.horizontal,input.clockwise,input.soft_drop,input.hard_drop);
            if(step==study_round::Step::invalid || step==study_round::Step::stopped)
                throw std::logic_error("planner returned an invalid trace");
            states.push_back(study_hash::state_bytes(predicted));
        }
        for(const auto& state:states)
            if(!state.ok())throw std::runtime_error("route state encoding overflow");
        if(!same(states.back(),route->result))throw std::logic_error("planner trace/result mismatch");
        // Build the entire candidate before publishing ready.
        inputs_=route->inputs;
        states_=std::move(states);
        status_=Status::ready;
        return true;
    }

    std::optional<study_input::Mask> issue(const study_round::Round& live) noexcept {
        if(status_!=Status::ready)return std::nullopt;
        if(!same(states_[cursor_],live)) {status_=Status::stale;return std::nullopt;}
        status_=Status::awaiting_feedback;
        return inputs_[cursor_];
    }

    bool observe(const study_round::Round& live) noexcept {
        if(status_!=Status::awaiting_feedback)return false;
        if(!same(states_[cursor_+1],live)) {status_=Status::stale;return false;}
        ++cursor_;
        status_=cursor_==inputs_.size()?Status::complete:Status::ready;
        return true;
    }
private:
    std::vector<study_input::Mask> inputs_;
    std::vector<study_hash::RoundBytes> states_;
    std::size_t cursor_=0;
    Status status_=Status::idle;
};
}
