#include "net/thread_relay_channel.h"
#include "net/loop_relay_channels.h"
#include "tests/authority_fixture.h"
#include <atomic>
#include <future>
#include <iostream>
#include <vector>
using namespace study_net;
using namespace study_authority_test;
using namespace std::chrono_literals;
using Stage=ResultHandoff::Stage;
inline Reply accepted(const MatchRecord& r){return {Reply::confirmed,{r.key,1,r.player_a,r.player_b}};}
template<class Receive,class View> void finish_game(Receive receive,View view){
    for(std::uint32_t t=0;view().match.state==MatchState::incomplete;++t){
        require(t<1000);
        require(receive(11,frame(t,{0})).input==InputDecision::stored);
        require(receive(22,frame(t,{study_input::drop})).input==InputDecision::stored);
    }
    require(view().match.state==MatchState::finished);
}
struct Release {
    std::promise<void> promise;bool opened=false;
    void open(){if(!opened){opened=true;promise.set_value();}}
    ~Release(){open();}
};
int main(){try{
    const PacePolicy policy{1000,1000,1000};auto clock=[] {return std::int64_t(0);};
    const MatchRecord record{7,9,11,22,1,0,0,0,0,MatchRecord::a};
    ResultHandoff gate(7);auto bad=record;bad.key=8;
    bool threw=false;try{gate.claim(bad);}catch(const std::invalid_argument&){threw=true;}
    require(threw && gate.stage()==Stage::open);
    require(gate.claim(record) && !gate.claim(record));
    require(!gate.complete(8,accepted(record)) && gate.stage()==Stage::inflight);
    auto bad_reply=accepted(record);++bad_reply.receipt.player_b;
    require(gate.complete(7,bad_reply) && gate.stage()==Stage::unconfirmed);
    require(!gate.complete(7,accepted(record)));
    // Both receiving threads share one match; the clock is invoked under its lock.
    ThreadRelayChannel threaded(7,9,11,22,77,policy,0);
    std::atomic<int> active_clock{0};std::atomic<bool> overlap{false};
    auto checked_clock=[&]{if(active_clock.fetch_add(1)!=0)overlap=true;std::this_thread::yield();--active_clock;return std::int64_t(0);};
    std::vector<std::thread> readers;
    for(auto actor:{11u,22u})readers.emplace_back([&,actor]{for(int i=0;i<20;++i)threaded.receive(actor,frame(0,{0}),checked_clock);});
    for(auto& t:readers)t.join();
    require(!overlap && threaded.view().match.ticks==1);
    // Continue from the already paired first neutral tick.
    for(std::uint32_t t=1;threaded.view().match.state==MatchState::incomplete;++t){
        require(t<1000);threaded.receive(11,frame(t,{0}),clock);threaded.receive(22,frame(t,{study_input::drop}),clock);
    }
    std::atomic<int> sends{0},claims{0};std::promise<void> entered;Release release;
    const auto gate_future=release.promise.get_future().share();
    auto sender=[&](const MatchRecord& r){++sends;require(threaded.view().saving==Stage::inflight);entered.set_value();gate_future.wait();return accepted(r);};
    std::thread first([&]{if(threaded.finalize(sender))++claims;});
    const bool entered_on_time=entered.get_future().wait_for(2s)==std::future_status::ready;
    if(!entered_on_time){release.open();first.join();require(false);}
    // Read and duplicate finalization complete even while storage is blocked.
    require(threaded.view().saving==Stage::inflight);
    std::vector<std::thread> contenders;
    for(int i=0;i<8;++i)contenders.emplace_back([&]{if(threaded.finalize(sender))++claims;});
    for(auto& t:contenders)t.join();
    release.open();first.join();
    require(claims==1 && sends==1 && threaded.view().saving==Stage::confirmed);
    require(threaded.receive(11,frame(0,{0}),clock).input==InputDecision::inactive);
    ThreadRelayChannel incomplete(8,9,11,22,77,policy,0);
    require(incomplete.finalize([&](const MatchRecord& r){++sends;return accepted(r);}));
    require(sends==1 && incomplete.view().saving==Stage::not_eligible && incomplete.view().match.state==MatchState::incomplete);
    // Owner loop with slow storage, capacity rejection and removal before completion.
    std::atomic<bool> worker_other{false};const auto owner=std::this_thread::get_id();
    LoopRelayChannels loop(1,1,[]{});Release slow;const auto ready=slow.promise.get_future().share();
    const auto old=loop.open(10,9,11,22,77,policy,0);
    auto play=[&](auto id){finish_game([&](auto actor,const Frame& f){return loop.receive(id,actor,f,clock);},[&]{return *loop.view(id);});};
    play(old);std::promise<void> working;
    require(loop.finalize(old,[&](const MatchRecord& r){worker_other=std::this_thread::get_id()!=owner;working.set_value();ready.wait();return accepted(r);}));
    require(working.get_future().wait_for(2s)==std::future_status::ready);
    require(loop.view(old)->saving==Stage::inflight);
    const auto full=loop.open(11,9,11,22,77,policy,0);play(full);
    require(loop.finalize(full,accepted) && loop.view(full)->saving==Stage::unconfirmed);
    require(loop.erase(old));const auto replacement=loop.open(12,9,11,22,77,policy,0);require(replacement!=old);
    bool wrong_owner=false;std::thread stranger([&]{try{loop.view(replacement);}catch(const std::logic_error&){wrong_owner=true;}});stranger.join();require(wrong_owner);
    slow.open();loop.shutdown();
    require(worker_other && loop.view(replacement)->saving==Stage::open);
    require(loop.open(13,9,11,22,77,policy,0)==0);
    // Shutdown runs accepted completion on the owner before destroying channels.
    LoopRelayChannels draining(2,4,[]{});
    auto create_finished=[&](std::uint64_t key){const auto id=draining.open(key,9,11,22,77,policy,0);finish_game([&](auto a,const Frame& f){return draining.receive(id,a,f,clock);},[&]{return *draining.view(id);});return id;};
    const auto ok=create_finished(20),failed=create_finished(21),mismatch=create_finished(22);
    require(draining.finalize(ok,accepted));require(!draining.finalize(ok,accepted));
    require(draining.finalize(failed,[](const MatchRecord&)->Reply{throw std::runtime_error("storage uncertainty");}));
    require(draining.finalize(mismatch,[](const MatchRecord& r){auto reply=accepted(r);reply.receipt.row=0;return reply;}));
    draining.shutdown();
    require(draining.view(ok)->saving==Stage::confirmed && draining.view(failed)->saving==Stage::unconfirmed && draining.view(mismatch)->saving==Stage::unconfirmed);
    std::cout<<"thread claims, clock serialization, I/O outside lock, owner completion, capacity, stale ID, shutdown passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
