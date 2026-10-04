#include "bot/paced_driver.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
void require(bool ok){if(!ok)throw std::runtime_error("paced driver contract");}
int main() {
    using study_paced::Event;
    // Fixture conditions: one left move then a drop, gravity disabled by a long interval.
    study_python::Session live(91,1000),oracle=live;
    study_paced::Driver driver({3,2,8});int calls=0;std::vector<int> moves;
    auto left=[&](const study_python::Session& state,int& action) {
        ++calls;const auto pose=*state.pose();action=*study_actions::encode(pose.column-1,pose.quarter);return true;
    };
    for(int tick=0;tick<8;++tick) {
        const auto result=driver.tick(live,left);oracle.step(result.mask);
        require(oracle.state_bytes()==live.state_bytes());
        if(result.mask)moves.push_back(tick);
        require(tick==7 ? result.event==Event::dropped : result.event!=Event::dropped);
    }
    require((moves==std::vector<int>{2,7}) && calls==1 && driver.age_for_gates()==0);
    for(int i=0;i<2;++i)require(driver.tick(live,left).mask==0);
    require(calls==1);driver.tick(live,left);require(calls==2);
    // An all-empty input stream still advances gravity, even when no policy is called.
    study_python::Session falling(44,1),direct=falling;
    study_paced::Driver delayed({30,180,600});int selections=0,locks=0;
    for(int t=0;t<80 && !falling.finished();++t) {
        const auto result=delayed.tick(falling,[&](const auto&,int&){++selections;return false;});
        require(result.mask==0);const auto step=direct.step(0);
        require(result.step==step && direct.state_bytes()==falling.state_bytes());
        if(result.event==Event::gravity_lock){++locks;require(delayed.age_for_gates()==0);}
    }
    require(locks>1 && selections==0);
    // Failed selection attempts are spaced; they do not stop the rule clock.
    study_python::Session failure(12,1000),failure_oracle=failure;
    study_paced::Driver retry({3,0,1});std::vector<int> attempts;
    for(int t=0;t<10;++t) {
        auto result=retry.tick(failure,[&](const auto&,int&){attempts.push_back(t);return false;});
        require(result.mask==0);failure_oracle.step(0);
        require(failure.state_bytes()==failure_oracle.state_bytes());
    }
    require((attempts==std::vector<int>{0,3,6,9}));
    const auto state=failure.state_bytes();const int age=retry.age_for_gates();
    try{retry.reset({0,0,1});require(false);}catch(const std::invalid_argument&){}
    require(retry.age_for_gates()==age && failure.state_bytes()==state);
    // A callback exception must not partially advance the driver or Session.
    study_paced::Driver atomic({1,0,1});
    bool pickerRejected=false;
    try{atomic.tick(failure,[](const auto&,int&)->bool{throw std::runtime_error("picker");});}
    catch(const std::runtime_error& e){pickerRejected=std::string(e.what())=="picker";}
    require(pickerRejected && atomic.age_for_gates()==0 && failure.state_bytes()==state);
    auto bad=atomic.tick(failure,[](const auto&,int& action){action=std::numeric_limits<int>::max();return true;});
    require(bad.event==Event::invalid_target && bad.mask==0);
    failure.step(0); // Deliberately violate the single tick-owner contract.
    const auto changed=failure.state_bytes();
    try{atomic.tick(failure,left);require(false);}catch(const std::logic_error&){}
    require(failure.state_bytes()==changed);
    atomic.reset({1,0,1});atomic.tick(failure,[](const auto&,int&){return false;});
    std::cout<<"pacing timelines, gravity, retries, reset and failure preservation passed\n";
}
