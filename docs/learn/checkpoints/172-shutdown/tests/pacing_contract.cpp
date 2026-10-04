#include "net/paced_match.h"
#include "tests/authority_fixture.h"
#include <iostream>
#include <limits>
using namespace study_net;
using namespace study_authority_test;

int main(int argc,char**) {try {
    if(argc>1){ // Batch oracle protocol: rate lead maximum elapsed, one tuple per line.
        std::uint32_t rate;std::uint64_t lead,maximum,elapsed;
        while(std::cin>>rate>>lead>>maximum>>elapsed)
            std::cout<<TickAllowance({rate,lead,maximum}).allowed(elapsed)<<'\n';
        return 0;
    }
    for(auto policy:{PacePolicy{0,0,1},{1000000001,0,1},{1,0,0},{1,2,1},{1,0,UINT64_MAX}}){
        bool threw=false;try{TickAllowance rejected(policy);}catch(const std::invalid_argument&){threw=true;}
        require(threw);
    }
    TickAllowance allowance({10,2,100});
    require(allowance.allowed(0)==2 && allowance.allowed(99999999)==2);
    require(allowance.allowed(100000000)==3 && allowance.allowed(UINT64_MAX)==100);
    require(TickAllowance({1,1,1}).allowed(0)==1);
    require(TickAllowance({1000000000,0,std::uint64_t(UINT32_MAX)+1}).allowed(UINT64_MAX)==std::uint64_t(UINT32_MAX)+1);
    const PacePolicy policy{10,1,100};
    for(auto now:{std::int64_t(99999999),std::int64_t(100000000)}){
        PacedMatch match(9,11,22,77,policy,0);
        const auto before=match.state();
        const auto reply=match.submit(11,frame(0,{0,0}),now);
        require(reply.failure==(now<100000000?PaceFailure::too_fast:PaceFailure::none));
        require(match.result().ticks==0);same_duel(before,match.state());
        if(now==100000000){require(match.submit(22,frame(0,{0,0}),now).input==InputDecision::stored);require(match.result().ticks==2);}
        else require(!match.record(7) && match.submit(22,frame(0,{0}),1000000000).input==InputDecision::inactive);
    }
    // The allowance is computed from fixed activation, never replenished per packet.
    PacedMatch burst(9,11,22,77,{10,2,100},0);
    require(burst.submit(11,frame(0,{0}),0).input==InputDecision::stored);
    require(burst.submit(22,frame(0,{0}),0).input==InputDecision::stored);
    require(burst.submit(11,frame(1,{0}),0).input==InputDecision::stored);
    require(burst.submit(22,frame(1,{0}),0).input==InputDecision::stored);
    require(burst.result().ticks==2);
    require(burst.submit(11,frame(2,{0}),0).failure==PaceFailure::too_fast);
    require(burst.result().ticks==2 && !burst.record(8));
    // Scope checks run before timing. Wrong-round and outsider packets cannot poison the clock.
    PacedMatch scope(9,11,22,77,policy,0);
    require(scope.submit(0,frame(0,{0}),-1).input==InputDecision::unauthenticated);
    require(scope.submit(99,frame(0,{0,0}),-1).input==InputDecision::not_participant);
    auto wrong=frame(0,{0});wrong.payload[0]=10;
    require(scope.submit(11,wrong,INT64_MAX).input==InputDecision::wrong_round);
    wrong.type=42;require(scope.submit(11,wrong,-1).input==InputDecision::forbidden_type);
    require(scope.submit(11,frame(0,{0}),0).input==InputDecision::stored);
    require(scope.submit(11,frame(0,{0}),100000000).input==InputDecision::duplicate);
    require(scope.submit(22,frame(0,{0}),99999999).failure==PaceFailure::clock_regression);
    PacedMatch negative(9,11,22,77,policy,100);
    require(negative.submit(11,frame(0,{0}),99).failure==PaceFailure::clock_regression);
    PacedMatch extremes(9,11,22,77,policy,INT64_MIN);
    require(extremes.submit(11,frame(0,{0}),INT64_MAX).input==InputDecision::stored);
    PacedMatch total(9,11,22,77,{10,1,2},0);
    require(total.submit(11,frame(0,{0,0,0}),INT64_MAX).failure==PaceFailure::total_limit);
    require(total.result().ticks==0);
    PacedMatch budget(9,11,22,77,{10,2,2},0);
    require(budget.submit(11,frame(0,{0,0}),0).input==InputDecision::stored);
    require(budget.submit(22,frame(0,{0,0}),0).input==InputDecision::stored);
    require(budget.result().state==MatchState::budget_exhausted && !budget.record(1));
    // A bounded future hole is stored, but cannot run past a missing current input.
    PacedMatch order(9,11,22,77,{10,100,100},0);
    require(order.submit(11,frame(1,{0}),0).input==InputDecision::stored);
    require(order.submit(22,frame(1,{0}),0).input==InputDecision::stored);
    require(order.result().ticks==0);
    require(order.submit(11,frame(0,{0}),0).input==InputDecision::stored);
    require(order.submit(22,frame(0,{0}),0).input==InputDecision::stored);
    require(order.result().ticks==2);
    require(order.submit(11,frame(0,{0}),0).input==InputDecision::stale);
    require(order.submit(11,frame(2+TickInputs::capacity,{0}),INT64_MAX).input==InputDecision::too_far);
    require(order.submit(11,frame(2,{0}),0).input==InputDecision::stored); // Denial did not move clock.
    require(order.submit(11,frame(2,{study_input::drop}),0).input==InputDecision::conflict);
    require(order.result().state==MatchState::invalid);
    // Full-byte masks, exact length, wire overflow: validate before any rule change.
    for(unsigned mask=0;mask<256;++mask){
        PacedMatch match(9,11,22,77,policy,0);auto f=frame(0,{0});f.payload[kRoundHeaderBytes]=mask;
        const auto reply=match.submit(11,f,0);
        require(reply.input==(study_input::valid(mask)?InputDecision::stored:InputDecision::malformed));
        require(match.result().ticks==0);
    }
    for(int kind=0;kind<5;++kind){
        PacedMatch match(9,11,22,77,policy,0);auto f=frame(0,{0});
        if(kind==0)--f.size;
        if(kind==1)++f.size;
        if(kind==2){f.payload[12]=0;f.size=kRoundHeaderBytes;}
        if(kind==3)f.payload[12]=kMaxBatchInputs+1;
        if(kind==4){f=frame(0,{0,0});for(int n=8;n<12;++n)f.payload[n]=255;}
        require(match.submit(11,f,0).input==InputDecision::malformed && !match.record(1));
    }
    // Batch rejection must not leave a prefix in the window.
    TickInputs window;const std::uint8_t masks[]{0,study_input::drop};
    require(window.put(Side::host,1,0)==Put::stored);
    require(window.put_batch(Side::host,0,masks,2)==Put::conflict);
    require(window.put(Side::peer,0,0)==Put::stored);
    std::uint8_t a=7,b=8;require(!window.peek(a,b) && a==7 && b==8);
    std::cout<<"input scope, integer pacing, rollback, masks, windows and terminal gates passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
