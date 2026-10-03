#include "tests/reactor_fixture.h"
#include <atomic>
#include <cstdio>
#include <limits>
#include <thread>
using namespace study_net;
using namespace study_fixture;
void registry_contract() {
    Registrations book;
    require(!book.add(Runtime::kInvalid,Read)&&!book.add(9,8),"bad registration");
    auto first=book.add(9,Read);require(bool(first),"first");
    require(!book.add(9,Write)&&!book.modify(*first,8),"duplicate/mask");
    require(book.modify(*first,0)&&book.find(*first)->interest==0,"pause");
    require(book.remove(*first)&&!book.find(*first)&&!book.remove(*first),"remove");
    auto second=book.add(9,Write);require(second&&*second!=*first,"fd reused, identity fresh");
    for(unsigned i=1;i<16;++i)require(bool(book.add(9+i,Read)),"fill");
    require(!book.add(100,Read),"capacity");
    Registrations last((std::numeric_limits<Registration>::max)());
    auto max=last.add(0,0);require(max&&*max==(std::numeric_limits<Registration>::max)(),"last id and fd0");
    require(last.remove(*max)&&!last.add(0,0),"id exhaustion never wraps");
}
void backend_contract() {
    auto reactor=make_poll_reactor();require(bool(reactor),"factory");
    auto a=pair(),b=pair();auto aid=reactor->watch(a.receiver,Read),bid=reactor->watch(b.receiver,Read);
    require(aid&&bid&&*aid!=*bid,"two subscriptions");
    require(reactor->poll(-1).state==PollState::error,"wait range");
    require(reactor->poll(0).state==PollState::timeout,"empty observation");
    send(a.sender,11);send(b.sender,22);
    const auto batch=observe_until(*reactor,2);require(batch.events.size()==2,"two ready");
    require(reactor->unwatch(*bid),"unwatch second");
    auto fresh=reactor->watch(b.receiver,Read);require(fresh&&*fresh!=*bid&&!reactor->current(*bid),"old batch stale");
    require(reactor->change(*aid,0),"pause read");
    const auto paused=observe_until(*reactor,1);require(paused.events[0].id==*fresh,"paused fd excluded");
    require(reactor->change(*aid,Read|Write),"read/write independent");
    const auto both=observe_until(*reactor,2);
    bool saw=false;for(const auto&e:both.events)if(e.id==*aid)saw=e.readable&&e.writable;
    require(saw,"combined interests");
    require(reactor->unwatch(*aid)&&reactor->unwatch(*fresh),"remove before socket destruction");
    require(reactor->wake(),"wake before wait");auto wake=reactor->poll(1000);
    require(wake.woken&&wake.events.empty()&&wake.state==PollState::events,"wake not connection event");
    // Queue/flag is authoritative; wake itself carries no application work.
    std::atomic_bool work{false};
    std::thread producer([&]{work.store(true,std::memory_order_release);reactor->wake();});
    auto crossing=reactor->poll(1000);producer.join();
    require(crossing.woken&&work.load(std::memory_order_acquire),"cross thread publication");
}
void callback_contract() {
    CallbackLoop loop(make_poll_reactor());
    auto a=pair(),b=pair();unsigned calls=0;Registration aid=0,bid=0;
    auto life=std::make_shared<int>(41);std::weak_ptr<int> weak=life;
    auto first=loop.attach(std::move(a.receiver),Read,[hold=life,&calls,&bid](CallbackLoop& owner,Socket&,const ReadyEvent& e){
        ++calls;require(owner.close(bid),"close other callback");
        require(owner.close(e.id),"close self callback");
        require(*hold==41,"closure alive after self removal");
    });life.reset();require(bool(first),"first callback");aid=*first;
    auto second=loop.attach(std::move(b.receiver),Read,[&](CallbackLoop&,Socket&,const ReadyEvent&){calls+=100;});
    require(bool(second),"second callback");bid=*second;
    send(a.sender,1);send(b.sender,2);
    ReadyBatch batch;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    do{batch=loop.observe(20);}while(batch.events.size()<2&&std::chrono::steady_clock::now()<deadline);
    require(batch.events.size()==2,"callback batch");
    // Callback ordering is an application decision, never a backend guarantee.
    std::sort(batch.events.begin(),batch.events.end(),[&](const auto&x,const auto&y){return x.id<y.id;});
    loop.dispatch(batch);require(calls==1&&weak.expired()&&!loop.current(aid)&&!loop.current(bid),"callback lifetime/stale skip");
    auto replacement=pair();unsigned replacement_calls=0;
    auto next=loop.attach(std::move(replacement.receiver),Read,[&](CallbackLoop&,Socket&,const ReadyEvent&){++replacement_calls;});
    require(next&&*next!=aid&&*next!=bid,"replacement identity");
    loop.dispatch(batch);require(replacement_calls==0,"old event cannot reach replacement");
    require(loop.close(*next),"replacement removed");
    auto exceptional=pair();auto ex=loop.attach(std::move(exceptional.receiver),Read,[](CallbackLoop& owner,Socket&,const ReadyEvent&e){
        owner.close(e.id);throw std::runtime_error("handler failure");
    });require(bool(ex),"exceptional attach");
    bool threw=false;try{loop.dispatch({PollState::events,{{*ex,true,false,false}},false,0});}catch(const std::runtime_error&){threw=true;}
    require(threw&&!loop.current(*ex),"exception propagates with removed node");
    loop.dispatch({}); // Guard restored despite exception.
    auto recursive=pair();auto re=loop.attach(std::move(recursive.receiver),Read,[](CallbackLoop&owner,Socket&,const ReadyEvent&e){
        bool blocked=false;try{owner.observe(0);}catch(const std::logic_error&){blocked=true;}
        require(blocked,"recursive observe rejected");owner.close(e.id);
    });require(bool(re),"recursive attach");loop.dispatch({PollState::events,{{*re,true,false,false}},false,0});
}
int main(){Runtime runtime;if(!runtime.ready())return 2;try{registry_contract();backend_contract();callback_contract();}
catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
std::puts("Reactor contracts: registration identity, masks, pause, batch removal, callback lifetime, wake and exception cleanup passed");}
