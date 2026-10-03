#include "net/delayed_lockstep.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::abort();}} while(0)
using namespace study_net;
bool same(const study_round::Round& a,const study_round::Round& b) {
    const auto x=study_hash::state_bytes(a),y=study_hash::state_bytes(b);
    if(!x.ok()||!y.ok()||x.size()!=y.size())return false;
    for(std::size_t i=0;i<x.size();++i)if(x.data()[i]!=y.data()[i])return false;
    return true;
}
int main() {
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},77);CHECK(round);
    const study_combat::Duel initial(*round,*round);
    CHECK(!DelayedLockstep::create(initial,static_cast<Side>(9),2));
    for(unsigned d=0;d<256;++d) {
        auto g=DelayedLockstep::create(initial,Side::host,d);
        CHECK(bool(g)==(d<=30));
        if(!g)continue;
        CHECK(g->local_limit()==-1-static_cast<std::int64_t>(d));
        CHECK(g->advance()==Advance::waiting);
        for(unsigned t=0;t<=d;++t) {
            CHECK(g->capture(0)==Put::stored && g->receive(t,0)==Put::stored);
            const auto a=g->advance();
            CHECK(a==(t==d?Advance::advanced:Advance::waiting));
        }
        CHECK(g->next_tick()==1 && g->advance()==Advance::waiting);
    }
    auto full=DelayedLockstep::create(initial,Side::host,2);CHECK(full);
    CHECK(full->capture(256)==Put::invalid && full->next_capture_tick()==0);
    for(unsigned t=0;t<32;++t)CHECK(full->capture(0)==Put::stored);
    CHECK(full->capture(0)==Put::too_far && full->next_capture_tick()==32);
    CHECK(full->receive(31,0)==Put::stored && full->advance()==Advance::waiting);
    CHECK(same(full->state().left(),*round));
    CHECK(full->receive(0,0)==Put::stored && full->advance()==Advance::advanced);
    CHECK(full->capture(0)==Put::stored && full->next_capture_tick()==33);
    auto batch=DelayedLockstep::create(initial,Side::host,2);CHECK(batch);
    const std::uint8_t values[]={0,0,0};
    CHECK(batch->receive(2,1)==Put::stored);
    CHECK(batch->receive_batch(0,values,3)==Put::conflict);
    CHECK(batch->receive(0,2)==Put::stored); // Failed prefix not committed.
    // The same tick-indexed history yields identical states under either delay.
    for(unsigned d:{0u,2u,30u}) {
        auto host=DelayedLockstep::create(initial,Side::host,d);
        auto peer=DelayedLockstep::create(initial,Side::peer,d);CHECK(host&&peer);
        study_combat::Duel direct=initial;
        for(unsigned t=0;t<32;++t) {
            const unsigned h=t%3==0?study_input::left:0;
            const unsigned p=t%3==0?study_input::right:0;
            CHECK(host->capture(h)==Put::stored && peer->capture(p)==Put::stored);
            CHECK(host->receive(t,p)==Put::stored && peer->receive(t,h)==Put::stored);
            const auto a=host->advance(),b=peer->advance();CHECK(a==b);
            if(t<d) {CHECK(a==Advance::waiting);continue;}
            CHECK(a==Advance::advanced);
            const unsigned tick=t-d;
            CHECK(direct.tick(*study_input::decode(tick%3==0?study_input::left:0),*study_input::decode(tick%3==0?study_input::right:0)));
            CHECK(same(host->state().left(),direct.left()) && same(host->state().right(),direct.right()));
            CHECK(same(peer->state().left(),direct.left()) && same(peer->state().right(),direct.right()));
        }
        CHECK(host->next_tick()==32-d && host->advance()==Advance::waiting);
    }
    // A longer arrival gap exceeds the reserve; no neutral synthesis.
    auto stalled=DelayedLockstep::create(initial,Side::host,2);CHECK(stalled);
    for(unsigned t=0;t<6;++t) {
        CHECK(stalled->capture(0)==Put::stored);
        if(t<3)CHECK(stalled->receive(t,0)==Put::stored);
        while(stalled->advance()==Advance::advanced){}
    }
    CHECK(stalled->next_tick()==3 && stalled->advance()==Advance::waiting);
    CHECK(stalled->receive(3,0)==Put::stored && stalled->advance()==Advance::advanced);
    // Final u32 tick is consumable at D0; D2 holds its tail without wrapping.
    for(unsigned d:{0u,2u}) {
        auto edge=DelayedLockstep::create(initial,Side::host,d,UINT32_MAX);CHECK(edge);
        CHECK(edge->capture(0)==Put::stored && edge->receive(UINT32_MAX,0)==Put::stored);
        CHECK(edge->capture(0)==Put::exhausted && edge->next_capture_tick()==std::uint64_t(UINT32_MAX)+1);
        CHECK(edge->advance()==(d==0?Advance::advanced:Advance::waiting));
        CHECK(edge->advance()==(d==0?Advance::exhausted:Advance::waiting));
    }
    auto finish=DelayedLockstep::create(initial,Side::host,2);CHECK(finish);
    bool done=false;
    for(unsigned t=0;t<200;++t) {
        CHECK(finish->capture(study_input::drop)==Put::stored && finish->receive(t,0)==Put::stored);
        const auto a=finish->advance();
        CHECK(a==(t<2?Advance::waiting:Advance::advanced));
        if(finish->state().left().finished()) {
            const auto tick=finish->next_tick();
            CHECK(finish->advance()==Advance::finished && finish->next_tick()==tick);
            done=true;break;
        }
    }
    CHECK(done);
    std::puts("DELAY_CONTRACT_OK");
}
