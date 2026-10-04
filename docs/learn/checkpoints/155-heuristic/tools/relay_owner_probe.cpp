#include "net/thread_relay_channel.h"
#include "net/loop_relay_channels.h"
#include "tests/authority_fixture.h"
#include "meta/sqlite_results.h"
#include <atomic>
#include <iostream>
#include <thread>
using namespace study_net;
using study_authority_test::frame;
using study_authority_test::require;
using Stage=ResultHandoff::Stage;
int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    study_meta::SqliteResults store(argv[1]);store.seed_demo();
    const PacePolicy policy{1000,1000,1000};auto clock=[] {return std::int64_t(0);};
    std::atomic<int> calls{0};
    auto sender=[&](const MatchRecord& r){
        ++calls;const auto result=store.put(r);
        return result.status==study_meta::SqlStatus::accepted?Reply{Reply::confirmed,result.receipt}:Reply{};
    };
    auto play=[&](auto receive,auto view){
        for(std::uint32_t tick=0;view().match.state==MatchState::incomplete;++tick){
            require(tick<1000);receive(101,frame(tick,{0}));receive(202,frame(tick,{study_input::drop}));
        }
        require(view().match.state==MatchState::finished);
    };
    ThreadRelayChannel threaded(901,9,101,202,77,policy,0);
    play([&](auto a,const Frame& f){return threaded.receive(a,f,clock);},[&]{return threaded.view();});
    std::thread a([&]{threaded.finalize(sender);}),b([&]{threaded.finalize(sender);});a.join();b.join();
    require(calls==1 && threaded.view().saving==Stage::confirmed);
    LoopRelayChannels loop(1,2,[]{});
    const auto id=loop.open(902,9,101,202,77,policy,0);
    play([&](auto actor,const Frame& f){return loop.receive(id,actor,f,clock);},[&]{return *loop.view(id);});
    require(loop.finalize(id,sender) && !loop.finalize(id,sender));loop.shutdown();
    require(calls==2 && loop.view(id)->saving==Stage::confirmed);
    require(store.awards(901).has_value() && store.awards(902).has_value());
    std::cout<<"thread + loop: server simulation -> real SQLite, each local claim sent once; replay same keys to check durable idempotency\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
