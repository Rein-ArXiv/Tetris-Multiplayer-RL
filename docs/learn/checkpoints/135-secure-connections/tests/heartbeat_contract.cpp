#include "net/heartbeat.h"
#include "net/thread_link.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <chrono>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"heartbeat line %d: %s\n",__LINE__,#x);std::abort();}}while(false)
using namespace study_net;
void codec(){Frame f;CHECK(encode_beat(false,0x0807060504030201ULL,f));CHECK(f.type==30 && f.size==8);
 for(unsigned i=0;i<8;++i)CHECK(f.payload[i]==i+1);
 Frame old=f;CHECK(!encode_beat(true,0,f));CHECK(f.type==old.type && f.payload==old.payload);
 for(unsigned type=0;type<256;++type)for(unsigned size=0;size<40;++size){Frame q=f;q.type=static_cast<std::uint8_t>(type);q.size=size;
  bool pong=true;std::uint64_t token=999;bool ok=decode_beat(q,pong,token);CHECK(ok==((type==30||type==31)&&size==8));
  if(ok){CHECK(pong==(type==31));CHECK(token==0x0807060504030201ULL);}else CHECK(pong && token==999);
 }
 CHECK(encode_beat(true,UINT64_MAX,f));bool pong=false;std::uint64_t token=0;CHECK(decode_beat(f,pong,token)&&pong&&token==UINT64_MAX);
 f.payload.fill(0);CHECK(!decode_beat(f,pong,token));CHECK(pong&&token==UINT64_MAX);
}
void clock_policy(){
 CHECK(BeatTiming{}.valid());CHECK((!BeatTiming{0,2,3}.valid()));CHECK((!BeatTiming{1,1,3}.valid()));
 Heartbeat h(100);std::uint64_t token=999;
 CHECK(h.status(100)==BeatHealth::waiting);CHECK(h.issue(100,token)&&token==1);
 CHECK(!h.pong(2,101));CHECK(!h.issue(1100,token));CHECK(h.status(2099)==BeatHealth::waiting);
 CHECK(h.status(2100)==BeatHealth::suspect);CHECK(!h.pong(2,2100));
 CHECK(h.pong(1,2100));CHECK(h.status(2100)==BeatHealth::healthy);
 CHECK(!h.pong(1,2101));CHECK(h.issue(2101,token)&&token==2);
 CHECK(h.status(2100)==BeatHealth::clock_error);CHECK(h.pong(2,2102));
 CHECK(!h.issue(3100,token)&&token==2);CHECK(h.issue(3101,token)&&token==3);
 CHECK(h.status(4101)==BeatHealth::healthy);CHECK(h.status(4102)==BeatHealth::suspect);
 CHECK(!h.pong(3,5102));CHECK(h.status(5102)==BeatHealth::expired);CHECK(!h.pong(3,5200));
 Heartbeat edge(0);CHECK(edge.issue(0,token));CHECK(edge.pong(token,2999));CHECK(edge.status(5998)==BeatHealth::suspect);CHECK(edge.status(5999)==BeatHealth::expired);
 Heartbeat none(0);CHECK(!none.pong(1,1));CHECK(none.status(3000)==BeatHealth::expired);
 Heartbeat high(UINT64_MAX-3000);CHECK(high.issue(UINT64_MAX-3000,token));CHECK(high.status(UINT64_MAX)==BeatHealth::expired);
}
void worker(){Runtime rt;CHECK(rt.ready());int e=0;std::uint16_t port=0;auto listener=listen_loopback(0,port,e);CHECK(listener.valid());
 auto a=connect_loopback(port,e);auto b=accept_one(listener,e);CHECK(a.valid()&&b.valid());
 ThreadLink x(std::move(a),true,{20,100,500}),y(std::move(b),true,{20,100,500});
 // Main deliberately does not pump the public receive queue. Worker control
 // frames must stay out of that queue and continue independently of main.
 std::this_thread::sleep_for(std::chrono::milliseconds(650));
 CHECK(x.confirmed_pongs()>=3 && y.confirmed_pongs()>=3);CHECK(!x.report()&&!y.report());Frame f;
 CHECK(x.receive(f)==QueueGet::empty && y.receive(f)==QueueGet::empty);
 bool threw=false;try{ThreadLink invalid(Socket{},true,{0,0,0});}catch(const std::invalid_argument&){threw=true;}CHECK(threw);
}
int main(){codec();clock_policy();worker();std::puts("heartbeat codec/time boundaries/correlation/replay/worker independent of main passed");}
