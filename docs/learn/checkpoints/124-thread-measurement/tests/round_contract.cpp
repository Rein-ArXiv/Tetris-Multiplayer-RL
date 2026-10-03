#include "net/round_play.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
using namespace study_net;
bool same(const Frame& a,const Frame& b){return a.type==b.type&&a.size==b.size&&a.payload==b.payload;}
bool same(const RoundBatch& a,const RoundBatch& b){return a.round==b.round&&a.inputs.first_tick==b.inputs.first_tick&&a.inputs.count==b.inputs.count&&a.inputs.masks==b.inputs.masks;}
bool same(const study_round::Round& a,const study_round::Round& b){
 auto x=study_hash::state_bytes(a),y=study_hash::state_bytes(b);
 if(!x.ok()||!y.ok()||x.size()!=y.size())return false;
 for(std::size_t i=0;i<x.size();++i)if(x.data()[i]!=y.data()[i])return false;
 return true;
}
Frame record(std::uint64_t round,std::uint32_t tick,unsigned mask){
 RoundBatch x; x.round=round;x.inputs.first_tick=tick;x.inputs.count=1;x.inputs.masks[0]=mask;
 Frame f;CHECK(encode_round_input(x,f));return f;
}
int main(){
 RoundBatch batch;batch.round=0x0807060504030201ULL;batch.inputs.first_tick=0x14131211;batch.inputs.count=2;batch.inputs.masks[0]=1;batch.inputs.masks[1]=31;
 Frame f;CHECK(encode_round_input(batch,f));const unsigned char expected[]={1,2,3,4,5,6,7,8,0x11,0x12,0x13,0x14,2,0,1,31};
 CHECK(f.size==sizeof expected&&f.type==41);for(unsigned i=0;i<sizeof expected;++i)CHECK(f.payload[i]==expected[i]);
 RoundBatch sentinel=batch;sentinel.round=99;
 for(unsigned kind=0;kind<256;++kind)for(unsigned size=0;size<40;++size){
  Frame bad=f;bad.type=kind;bad.size=size;RoundBatch out=sentinel;
  bool ok=decode_round_input(bad,out);CHECK(ok==(kind==41&&size==16));if(!ok)CHECK(same(out,sentinel));
 }
 for(unsigned mask=0;mask<256;++mask){Frame candidate=f;candidate.payload[15]=mask;RoundBatch out=sentinel;
  bool ok=decode_round_input(candidate,out);CHECK(ok==(mask<=31));if(!ok)CHECK(same(out,sentinel));}
 for(unsigned count=0;count<256;++count){Frame candidate=f;candidate.payload[12]=count;RoundBatch out=sentinel;
  CHECK(decode_round_input(candidate,out)==(count==2));if(count!=2)CHECK(same(out,sentinel));}
 auto invalid=batch;invalid.round=0;Frame original=f;CHECK(!encode_round_input(invalid,f)&&same(f,original));
 invalid=batch;invalid.inputs.first_tick=UINT32_MAX;CHECK(!encode_round_input(invalid,f)&&same(f,original));
 Frame zero=original;for(unsigned i=0;i<8;++i)zero.payload[i]=0;RoundBatch out=sentinel;CHECK(!decode_round_input(zero,out)&&same(out,sentinel));
 batch.round=UINT64_MAX;batch.inputs.first_tick=UINT32_MAX;batch.inputs.count=1;CHECK(encode_round_input(batch,f)&&decode_round_input(f,out)&&out.round==batch.round&&out.inputs.first_tick==batch.inputs.first_tick&&out.inputs.count==1&&out.inputs.masks[0]==batch.inputs.masks[0]);
 RoundGate gate;CHECK(!gate.start()&&!gate.finish()&&!gate.prepare(0)&&gate.round()==0);
 CHECK(gate.classify(0)==RoundScope::inactive&&gate.classify(1)==RoundScope::future_round);
 CHECK(gate.prepare(1)&&!gate.can_capture()&&!gate.prepare(2));
 CHECK(gate.classify(1)==RoundScope::current&&gate.classify(0)==RoundScope::old_round);
 CHECK(gate.start()&&gate.can_capture()&&!gate.start()&&gate.finish()&&!gate.finish());
 CHECK(gate.classify(1)==RoundScope::inactive&&!gate.prepare(1)&&gate.prepare(UINT64_MAX));
 CHECK(gate.finish()&&!gate.prepare(1)&&!gate.prepare(UINT64_MAX));
 const auto round=study_round::Round::create_seeded(study_grid::Grid{},77);CHECK(round);const study_combat::Duel duel(*round,*round);
 RoundPlay host,peer;Frame frame=original;
 for(unsigned i=0;i<1000;++i)CHECK(host.capture(1,frame).scope==RoundScope::inactive&&same(frame,original));
 CHECK(!host.game()&&host.prepare(1,duel,Side::host,0)&&peer.prepare(1,duel,Side::peer,0));
 CHECK(host.capture(1,frame).scope==RoundScope::inactive&&host.game()->next_capture_tick()==0);
 CHECK(host.start()&&host.capture(1,frame).input==Put::stored);
 Frame delayed=frame;
 CHECK(peer.receive(frame).input==Put::stored&&peer.advance()==Advance::waiting); // countdown staging
 CHECK(peer.start()&&peer.capture(0,frame).input==Put::stored&&host.receive(frame).input==Put::stored);
 CHECK(host.advance()==Advance::advanced&&peer.advance()==Advance::advanced);
 CHECK(same(host.game()->state().left(),peer.game()->state().left())&&same(host.game()->state().right(),peer.game()->state().right()));
 CHECK(peer.receive(delayed).input==Put::stale); // current round, already consumed tick
 CHECK(peer.finish()&&peer.receive(delayed).scope==RoundScope::inactive);
 CHECK(!peer.prepare(2,duel,static_cast<Side>(8),0)&&peer.gate().round()==1);
 CHECK(!peer.prepare(2,duel,Side::peer,31)&&peer.gate().round()==1);
 CHECK(peer.prepare(2,duel,Side::peer,0)&&peer.game()->next_tick()==0);
 CHECK(peer.receive(delayed).scope==RoundScope::old_round);
 CHECK(peer.receive(record(3,0,1)).scope==RoundScope::future_round&&peer.gate().round()==2);
 CHECK(peer.receive(zero).scope==RoundScope::invalid_frame);
 CHECK(peer.receive(record(2,0,2)).input==Put::stored);
 CHECK(peer.receive(record(2,0,2)).input==Put::duplicate);
 CHECK(peer.receive(record(2,0,1)).input==Put::conflict);
 CHECK(peer.start()&&peer.capture(0,frame).input==Put::stored&&peer.advance()==Advance::advanced);
 Lockstep reference(duel);CHECK(reference.submit(Side::host,0,2)==Put::stored&&reference.submit(Side::peer,0,0)==Put::stored&&reference.advance()==Advance::advanced);
 CHECK(same(peer.game()->state().left(),reference.state().left())&&same(peer.game()->state().right(),reference.state().right()));
 RoundPlay bounded;CHECK(bounded.prepare(5,duel,Side::host,0)&&bounded.start());
 for(unsigned i=0;i<32;++i)CHECK(bounded.capture(0,frame).input==Put::stored);
 const Frame last=frame;CHECK(bounded.capture(1,frame).input==Put::too_far&&same(frame,last)&&bounded.game()->next_capture_tick()==32);
 CHECK(bounded.capture(32,frame).input==Put::invalid&&same(frame,last));
 // Missing remote input never becomes a neutral record just because we poll.
 for(unsigned i=0;i<1000;++i)CHECK(bounded.advance()==Advance::waiting&&bounded.game()->next_tick()==0);
 RoundPlay end;CHECK(end.prepare(7,duel,Side::host,0,UINT32_MAX)&&end.start());
 CHECK(end.capture(0,frame).input==Put::stored&&end.receive(record(7,UINT32_MAX,0)).input==Put::stored&&end.advance()==Advance::advanced);
 CHECK(end.capture(0,frame).input==Put::exhausted&&end.advance()==Advance::exhausted&&end.gate().phase()==RoundPhase::ended);
 std::puts("round: codec domain/output preservation, lifecycle, countdown staging, old/future/current ticks, paired simulation, full capture and exhaustion passed");
}
