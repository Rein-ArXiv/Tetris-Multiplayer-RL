#include "bot/policy_match.h"
#include "simulation/duel.h"
#include <iostream>
#include <deque>
void require_at(bool ok,int line){if(!ok)throw std::runtime_error("policy match contract line "+std::to_string(line));}
#define require(ok) require_at((ok),__LINE__)
bool equal(const study_hash::RoundBytes& a,const study_hash::RoundBytes& b){
    return a.ok() && b.ok() && a.size()==b.size() && std::equal(a.data(),a.data()+a.size(),b.data());
}
bool greedy(const study_python::Session& state,int& action) {
    bool found=false;long best=0;
    for(int candidate:state.legal_actions()) {
        auto trial=state.clone();const auto event=trial.apply_action(candidate);const auto grid=trial.grid();
        long height=0,holes=0,bump=0;int previousHeight=0;const int rows=static_cast<int>(grid.size()),cols=static_cast<int>(grid.front().size());
        for(int col=0;col<cols;++col){bool seen=false;int columnHeight=0;for(int row=0;row<rows;++row){
            if(grid[row][col]){if(!seen){columnHeight=rows-row;height+=columnHeight;}seen=true;}else if(seen)++holes;
        }if(col)bump+=std::abs(columnHeight-previousHeight);previousHeight=columnHeight;}
        const long score=event.lines*20L-height*2-holes*8-bump;
        if(!found || score>best){found=true;best=score;action=candidate;}
    }
    return found;
}
int main() {
    using study_match::Verdict;
    study_characters::Character c{"bot",{"Bot","","",""},{"fixture",{1,0,1}}};
    auto first=[](const auto& state,int& action){auto legal=state.legal_actions();if(legal.empty())return false;action=legal.front();return true;};
    auto fail=[](const auto&,int&){return false;};
    study_match::Match live(77,c,bot::Mode::reward);
    study_combat::Duel oracle(live.human().round(),live.enemy().round());
    std::vector<std::uint8_t> inputs;std::size_t attacks=0;
    for(std::size_t tick=0;tick<2000 && !live.finished();++tick) {
        const auto input=static_cast<std::uint8_t>(tick%9==8?study_input::drop:0);
        const auto frame=live.tick(input,first,false);inputs.push_back(input);
        const auto result=oracle.tick(*study_input::decode(input),*study_input::decode(frame.bot.mask));require(result.has_value());
        attacks+=result->left_attack+result->right_attack;
        require(equal(study_hash::state_bytes(oracle.left()),study_hash::state_bytes(live.human().round())));
        require(equal(study_hash::state_bytes(oracle.right()),study_hash::state_bytes(live.enemy().round())));
        require(live.status().reward_eligible());
    }
    require(live.finished());
    const auto expected=live.human_won()?Verdict::victory:Verdict::not_victory;
    require(study_match::verify(77,c,inputs,2000,first)==expected);
    require(study_match::verify(77,c,{0},2000,fail)==Verdict::policy_unavailable);
    auto invalid=[](const auto&,int& action){action=-1;return true;};
    require(study_match::verify(77,c,{0},2000,invalid)==Verdict::policy_unavailable);
    study_match::Match practice(77,c,bot::Mode::reward);
    require(practice.tick(0,fail,true).source==study_match::Source::fallback);
    require(!practice.status().reward_eligible());
    practice.tick(0,first,true);require(!practice.status().reward_eligible());
    const auto before=practice.human().state_bytes(),enemyBefore=practice.enemy().state_bytes();
    bool rejected=false;try{practice.tick(255,first,true);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected && before==practice.human().state_bytes() && enemyBefore==practice.enemy().state_bytes());
    // Actual attacks, not only empty deltas: compare both Round states each tick.
    study_match::Match combat(1,c,bot::Mode::reward);
    study_combat::Duel combatOracle(combat.human().round(),combat.enemy().round());
    std::deque<study_input::Mask> queue;std::size_t exchanges=0,combatTicks=0;
    for(;combatTicks<1000 && !combat.finished();++combatTicks) {
        if(queue.empty()) {int action=-1;if(greedy(combat.human(),action)) {
            const auto route=combat.human().action_trace(action);queue.assign(route.begin(),route.end());
        }}
        const auto mask=queue.empty()?0:queue.front();if(!queue.empty())queue.pop_front();
        const auto frame=combat.tick(mask,greedy,false);
        const auto step=combatOracle.tick(*study_input::decode(mask),*study_input::decode(frame.bot.mask));require(step.has_value());
        exchanges+=step->left_attack+step->right_attack;
        require(equal(study_hash::state_bytes(combatOracle.left()),study_hash::state_bytes(combat.human().round())));
        require(equal(study_hash::state_bytes(combatOracle.right()),study_hash::state_bytes(combat.enemy().round())));
    }
    std::cerr<<"fixture combat ticks="<<combatTicks<<" attacks="<<exchanges<<" finished="<<combat.finished()<<"\n";
    require(exchanges>0);
    std::cout<<"combat oracle ticks="<<combatTicks<<" attack rows="<<exchanges<<'\n';
    // Specific authorized combat preserves pacing; arbitrary external ticks remain rejected.
    study_python::Session state(77);study_paced::Driver driver({3,2,8});driver.tick(state,first);
    const auto age=driver.age_for_gates();driver.queue_garbage(state,1);require(driver.age_for_gates()==age);
    driver.tick(state,first);state.step(0);rejected=false;
    try{driver.queue_garbage(state,1);}catch(const std::logic_error&){rejected=true;}require(rejected);
    std::cout<<"two-board oracle ticks="<<inputs.size()<<" attacks="<<attacks<<" human_win="<<live.human_won()
             <<" strict policy failure, fallback latch and state preservation passed\n";
}
