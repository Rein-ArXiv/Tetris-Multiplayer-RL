#include "net/send_budget.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
using namespace study_net;
using Clock=std::chrono::steady_clock;
using namespace std::chrono_literals;
struct Script {
    std::vector<SendAttempt> steps;
    std::size_t calls=0,offset=0,pauses=0;
    Clock::time_point time{};
    Clock::duration stepCost{};
    std::size_t cancelAfter=999;
    const std::uint8_t* base;
    std::size_t total;
    SendReport run(Clock::duration budget) {
        return send_until(base,total,time+budget,[&](const auto* data,std::size_t size) {
            CHECK(data==base+offset && size==total-offset && calls<steps.size());
            auto result=steps[calls++];time+=stepCost;
            if(result.state==SendState::progress && result.count>0 && result.count<=size && result.error==0) offset+=result.count;
            return result;
        },[&]{return time;},[&](auto duration){CHECK(duration>Clock::duration::zero() && duration<=1ms);++pauses;time+=duration;},[&]{return calls>=cancelAfter;});
    }
};
int main() {
    const std::uint8_t data[]={1,2,3,4,5,6};
    const SendAttempt p2{SendState::progress,2,0},p4{SendState::progress,4,0},block{SendState::would_block,0,0},interrupt{SendState::interrupted,0,0};
    auto make=[&](std::vector<SendAttempt> steps) {return Script{steps,0,0,0,{}, {},999,data,6};};
    auto script=make({p2,block,interrupt,p4});auto r=script.run(10ms);
    CHECK(r.outcome==SendOutcome::complete && r.accepted==6 && script.pauses==1);
    script=make({p2,{SendState::error,0,42}});r=script.run(10ms);CHECK(r.outcome==SendOutcome::error && r.accepted==2 && r.error==42);
    script=make({p2,p4});script.cancelAfter=1;r=script.run(10ms);CHECK(r.outcome==SendOutcome::cancelled && r.accepted==2 && script.calls==1);
    script=make({block,block,block});r=script.run(2500us);CHECK(r.outcome==SendOutcome::timed_out && r.accepted==0 && script.pauses==3 && script.time==Clock::time_point{}+2500us);
    script=make({p2,p2,p2});script.stepCost=3ms;r=script.run(5ms);CHECK(r.outcome==SendOutcome::timed_out && r.accepted==4 && script.calls==2);
    script=make({interrupt,interrupt,interrupt});script.stepCost=3ms;r=script.run(5ms);CHECK(r.outcome==SendOutcome::timed_out && r.accepted==0 && script.calls==2 && script.pauses==0);
    script=make({block});script.stepCost=6ms;r=script.run(5ms);CHECK(r.outcome==SendOutcome::timed_out && script.pauses==0);
    for(auto invalid: {SendAttempt{SendState::progress,0,0},SendAttempt{SendState::progress,7,0},SendAttempt{SendState::progress,2,1},SendAttempt{SendState::would_block,1,0},SendAttempt{SendState::interrupted,0,7},SendAttempt{static_cast<SendState>(99),0,0}}) {
        script=make({invalid});r=script.run(10ms);CHECK(r.outcome==SendOutcome::invalid_result && r.accepted==0);
    }
    script=make({});r=script.run(0ms);CHECK(r.outcome==SendOutcome::timed_out && script.calls==0);
    script=make({});script.base=nullptr;script.total=0;r=script.run(0ms);CHECK(r.outcome==SendOutcome::complete && script.calls==0);
    script=make({});script.base=nullptr;r=script.run(10ms);CHECK(r.outcome==SendOutcome::invalid_request && script.calls==0);
    script=make({{SendState::progress,6,0}});script.stepCost=6ms;r=script.run(5ms);CHECK(r.outcome==SendOutcome::complete && r.accepted==6);
    std::puts("Send budget: partial suffix, error prefix, cancellation, total deadline, interruptions and invalid results passed");
}
