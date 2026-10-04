#pragma once
#include "bot/characters.h"
#include "bot/paced_driver.h"
#include "bot/run_status.h"
#include <algorithm>

namespace study_match {
enum class Source { none, primary, fallback, no_legal, unavailable };
struct Frame { study_paced::Result bot; Source source=Source::none; };
enum class Verdict { victory, not_victory, policy_unavailable };

// One owner for both boards, pacing, post-tick combat and local fault history.
class Match {
public:
    Match(std::uint64_t seed,const study_characters::Character& character,bot::Mode mode)
        :human_(seed),enemy_(seed),driver_(character.behavior.pacing) {status_.reset(mode);}
    bool finished() const noexcept {return human_.finished() || enemy_.finished();}
    bool human_won() const noexcept {return !human_.finished() && enemy_.finished();}
    const bot::RunStatus& status() const noexcept {return status_;}
    const study_python::Session& human() const noexcept {return human_;}
    const study_python::Session& enemy() const noexcept {return enemy_;}

    template<class Picker> Frame tick(unsigned humanMask,Picker&& primary,bool allowFallback) {
        if(finished())throw std::logic_error("match already finished");
        auto next=*this;Frame frame;
        next.human_.step(humanMask);
        frame.bot=next.driver_.tick(next.enemy_,[&](const study_python::Session& observed,int& action) {
            const auto legal=observed.legal_actions();
            if(legal.empty()){frame.source=Source::no_legal;return false;}
            int selected=-1;bool ok=false;
            try {ok=primary(observed,selected);}catch(const std::exception&) {}
            if(ok && std::find(legal.begin(),legal.end(),selected)!=legal.end()) {
                frame.source=Source::primary;action=selected;return true;
            }
            next.status_.observe(ok?bot::Fault::invalid_target:bot::Fault::policy_failed);
            if(allowFallback){frame.source=Source::fallback;action=legal.front();return true;}
            frame.source=Source::unavailable;return false;
        });
        // Match the cumulative Duel's order: both ticks, then cross-deliver deltas.
        const auto humanAttack=next.human_.round().attack_sent();
        const auto enemyAttack=next.enemy_.round().attack_sent();
        if(humanAttack<next.humanDelivered_ || enemyAttack<next.enemyDelivered_)
            throw std::logic_error("attack total moved backwards");
        const auto toEnemy=humanAttack-next.humanDelivered_;
        const auto toHuman=enemyAttack-next.enemyDelivered_;
        constexpr auto cap=static_cast<std::uint64_t>(study_grid::Grid::kRows);
        if(toEnemy && !next.enemy_.finished())next.driver_.queue_garbage(next.enemy_,static_cast<int>(std::min(toEnemy,cap)));
        if(toHuman && !next.human_.finished())next.human_.add_garbage(static_cast<int>(std::min(toHuman,cap)));
        next.humanDelivered_=humanAttack;next.enemyDelivered_=enemyAttack;
        static_assert(std::is_nothrow_move_assignable_v<Match>);
        *this=std::move(next);return frame;
    }
private:
    study_python::Session human_,enemy_;
    study_paced::Driver driver_;
    bot::RunStatus status_;
    std::uint64_t humanDelivered_=0,enemyDelivered_=0;
};
// The caller supplies its trusted profile/policy, never a request-supplied verdict.
template<class Picker>
Verdict verify(std::uint64_t seed,const study_characters::Character& character,
               const std::vector<std::uint8_t>& inputs,std::size_t limit,Picker&& primary) {
    if(inputs.empty() || inputs.size()>limit)return Verdict::not_victory;
    Match match(seed,character,bot::Mode::reward);
    for(std::size_t i=0;i<inputs.size();++i) {
        if(!study_input::valid(inputs[i]))return Verdict::not_victory;
        match.tick(inputs[i],primary,false);
        if(!match.status().reward_eligible())return Verdict::policy_unavailable;
        if(match.finished())return i+1==inputs.size() && match.human_won() ? Verdict::victory : Verdict::not_victory;
    }
    return Verdict::not_victory;
}
}
