#include "net/lockstep.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::abort(); } } while(0)
using namespace study_net;
bool same(const study_round::Round& a, const study_round::Round& b) {
    const auto x=study_hash::state_bytes(a), y=study_hash::state_bytes(b);
    if (!x.ok() || !y.ok() || x.size()!=y.size()) return false;
    for (std::size_t i=0;i<x.size();++i) if(x.data()[i]!=y.data()[i]) return false;
    return true;
}
int main() {
    for (unsigned mask=0;mask<512;++mask) {
        TickInputs t;
        CHECK(t.put(Side::host,0,mask)==(mask<=31?Put::stored:Put::invalid));
    }
    TickInputs t;
    CHECK(t.put(static_cast<Side>(9),0,0)==Put::invalid);
    std::uint8_t h=77,p=88;
    CHECK(!t.peek(h,p) && h==77 && p==88 && !t.consume());
    CHECK(t.put(Side::host,0,0)==Put::stored);
    CHECK(t.put(Side::peer,1,2)==Put::stored);
    CHECK(!t.peek(h,p) && h==77 && p==88 && !t.consume());
    CHECK(t.put(Side::host,0,0)==Put::duplicate);
    CHECK(t.put(Side::host,0,1)==Put::conflict);
    CHECK(t.put(Side::peer,32,0)==Put::too_far);
    CHECK(t.put(Side::peer,31,0)==Put::stored);
    CHECK(t.put(Side::peer,0,0)==Put::stored);
    CHECK(t.peek(h,p) && h==0 && p==0 && t.consume() && t.next_tick()==1);
    CHECK(t.put(Side::host,0,0)==Put::stale);
    CHECK(t.put(Side::host,1,1)==Put::stored);
    CHECK(t.peek(h,p) && h==1 && p==2 && t.consume());
    for (unsigned i=2;i<31;++i) {
        CHECK(t.put(Side::host,i,0)==Put::stored);
        CHECK(t.put(Side::peer,i,0)==Put::stored);
        CHECK(t.consume());
    }
    CHECK(t.put(Side::host,31,0)==Put::stored && t.peek(h,p) && p==0 && t.consume());
    CHECK(!t.peek(h,p)); // Last slot cleared after every shift.
    const std::uint8_t batch[]={1,2,3};
    TickInputs b;
    CHECK(b.put(Side::host,2,4)==Put::stored);
    CHECK(b.put_batch(Side::host,0,batch,3)==Put::conflict);
    CHECK(b.put(Side::host,0,0)==Put::stored); // Failed batch inserted nothing.
    CHECK(b.put_batch(Side::peer,31,batch,2)==Put::too_far);
    CHECK(b.put(Side::peer,31,0)==Put::stored);
    CHECK(b.put_batch(Side::peer,UINT32_MAX,batch,2)==Put::invalid);
    CHECK(b.put_batch(Side::peer,0,nullptr,1)==Put::invalid);
    CHECK(b.put_batch(Side::peer,0,batch,0)==Put::invalid);
    CHECK(b.put_batch(Side::peer,0,batch,33)==Put::invalid);
    const std::uint8_t bad[]={0,32};
    CHECK(b.put_batch(Side::peer,0,bad,2)==Put::invalid);
    CHECK(b.put_batch(Side::peer,0,batch,3)==Put::stored);
    CHECK(b.put_batch(Side::peer,0,batch,3)==Put::duplicate);
    TickInputs edge(UINT32_MAX);
    CHECK(edge.put(Side::host,UINT32_MAX,0)==Put::stored);
    CHECK(edge.put(Side::peer,UINT32_MAX,0)==Put::stored && edge.consume());
    CHECK(edge.next_tick()==std::uint64_t(UINT32_MAX)+1 && edge.exhausted());
    CHECK(edge.put(Side::host,0,0)==Put::exhausted && !edge.consume());
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},77);
    CHECK(round);
    study_combat::Duel expected(*round,*round);
    Lockstep a(expected), other(expected);
    CHECK(a.advance()==Advance::waiting && same(a.state().left(),*round));
    CHECK(a.submit(Side::host,0,1)==Put::stored);
    CHECK(a.submit(Side::peer,1,0)==Put::stored);
    CHECK(a.advance()==Advance::waiting && a.next_tick()==0);
    CHECK(same(a.state().left(),*round) && same(a.state().right(),*round));
    CHECK(a.submit(Side::peer,0,2)==Put::stored);
    CHECK(other.submit(Side::peer,0,2)==Put::stored);
    CHECK(other.submit(Side::host,0,1)==Put::stored);
    CHECK(a.advance()==Advance::advanced && other.advance()==Advance::advanced);
    CHECK(expected.tick(*study_input::decode(1),*study_input::decode(2)));
    CHECK(same(a.state().left(),expected.left()) && same(a.state().right(),expected.right()));
    CHECK(same(a.state().left(),other.state().left()) && same(a.state().right(),other.state().right()));
    CHECK(a.next_tick()==1 && a.advance()==Advance::waiting);
    Lockstep end(expected,UINT32_MAX);
    CHECK(end.submit(Side::host,UINT32_MAX,0)==Put::stored);
    CHECK(end.submit(Side::peer,UINT32_MAX,0)==Put::stored);
    CHECK(end.advance()==Advance::advanced && end.advance()==Advance::exhausted);
    Lockstep game(expected);
    bool finished=false;
    for(unsigned i=0;i<200;++i) {
        CHECK(game.submit(Side::host,i,study_input::drop)==Put::stored);
        CHECK(game.submit(Side::peer,i,0)==Put::stored);
        CHECK(game.advance()==Advance::advanced);
        if(game.state().left().finished()) {
            const auto tick=game.next_tick();
            CHECK(game.advance()==Advance::finished && game.next_tick()==tick);
            finished=true;break;
        }
    }
    CHECK(finished);
    std::puts("INPUT_WINDOW_LOCKSTEP_OK");
}
