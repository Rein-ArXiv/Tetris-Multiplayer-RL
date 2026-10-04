#pragma once
#include "bindings/session.h"
#include "bot/pacing.h"
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace study_paced {
enum class Event { waiting, input, no_decision, invalid_target, blocked, gravity_lock, dropped, finished };
struct Result {
    study_input::Mask mask=0;
    study_round::Step step=study_round::Step::waiting;
    Event event=Event::waiting;
    bool decision_attempted=false;
};
// This owner drives every real tick of the Session, including empty input ticks.
// External state changes require an explicit reset before further tick calls.
class Driver {
public:
    explicit Driver(bot::Pacing config={}) : gate_(config) {}
    void reset(bot::Pacing config) {
        gate_.reset(config); // Reject before discarding the old route.
        target_.reset();expected_.reset();
    }
    int age_for_gates() const noexcept {return gate_.age_for_gates();}
    const bot::Pacing& config() const noexcept {return gate_.config();}

    template<class Picker> Result tick(study_python::Session& live,Picker&& picker) {
        if(expected_ && *expected_!=live.state_bytes())
            throw std::logic_error("Session changed outside paced driver; reset required");
        if(live.finished())return {0,study_round::Step::stopped,Event::finished,false};
        auto next=*this;
        auto candidate=live.clone();
        const auto result=next.advance(candidate,picker);
        next.expected_=candidate.state_bytes();
        static_assert(std::is_nothrow_move_assignable_v<study_python::Session>);
        static_assert(std::is_nothrow_move_assignable_v<Driver>);
        live=std::move(candidate);
        *this=std::move(next);
        return result;
    }
private:
    template<class Picker> Result advance(study_python::Session& state,Picker& picker) {
        Result result;
        const auto before=state.pose();
        if(!before)throw std::logic_error("live Session has no active piece");
        if(gate_.begin_tick()) {
            if(!target_) {
                result.decision_attempted=true;
                int action=-1;
                if(!picker(static_cast<const study_python::Session&>(state),action)) {
                    result.event=Event::no_decision;gate_.defer();
                } else if(!(target_=study_actions::decode(action))) {
                    result.event=Event::invalid_target;gate_.defer();
                }
            }
            if(target_) {
                if(before->quarter!=target_->quarter)result.mask=study_input::rotate;
                else if(before->column<target_->column)result.mask=study_input::right;
                else if(before->column>target_->column)result.mask=study_input::left;
                else if(gate_.drop_ready())result.mask=study_input::drop;
                if(result.mask)gate_.defer();
            }
        }
        result.step=state.step(result.mask); // Exactly one rule tick, even while waiting.
        if(result.step==study_round::Step::locked || result.step==study_round::Step::game_over) {
            result.event=result.mask==study_input::drop?Event::dropped:Event::gravity_lock;
            target_.reset();gate_.new_piece();
        } else if(result.mask) {
            const auto after=state.pose();
            const int column=before->column+(result.mask==study_input::right)-(result.mask==study_input::left);
            const int quarter=(before->quarter+1)%study_actions::kOrientations;
            const bool accepted=after &&
                (result.mask==study_input::rotate?after->quarter==quarter:after->column==column);
            if(result.mask==study_input::drop)throw std::logic_error("drop did not resolve a piece");
            result.event=accepted?Event::input:Event::blocked;
            if(!accepted)target_.reset();
        }
        return result;
    }
    bot::TickGate gate_;
    std::optional<study_actions::Target> target_;
    std::optional<std::vector<std::uint8_t>> expected_;
};
}
