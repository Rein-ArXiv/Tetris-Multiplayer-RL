#include "net/framing.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"admission line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
#define NET_WARN(x) do{}while(false)
using namespace net;
std::function<bool(const std::uint8_t*,std::size_t)> transport;
bool tcp_send_all(int,const std::uint8_t*data,std::size_t n){return transport(data,n);}
struct Session {
 std::mutex sendMu;std::deque<std::vector<std::uint8_t>> sendQ;std::size_t pendingSendBytes=0;
 std::atomic_bool connectionFailed{false},quit{false};int sock=0;
 void pushSend(std::vector<std::uint8_t>&& fr);
#ifndef BEFORE
 void drain(){bool hasActivity=false;
#include "send_drain.inc"
 (void)hasActivity;}
 void clear_round(){
#include "send_filter.inc"
 }
#endif
};
#include "send_admission.inc"
std::vector<std::uint8_t> packet(std::size_t n,MsgType type=MsgType::CHAT){std::vector<std::uint8_t> v(n,0);if(n>=3)v[2]=static_cast<std::uint8_t>(type);return v;}
int main(){
 Session full;for(unsigned i=0;i<256;++i)full.pushSend(packet(4096));CHECK(!full.connectionFailed);
 full.pushSend(packet(4096));CHECK(full.connectionFailed && full.quit);CHECK(full.sendQ.size()==256);
#ifndef BEFORE
 CHECK(full.pendingSendBytes==1024*1024);
 Session active;for(unsigned i=0;i<256;++i)active.pushSend(packet(4096));unsigned calls=0;
 transport=[&](const std::uint8_t*,std::size_t){++calls;CHECK(active.pendingSendBytes==1024*1024);active.pushSend(packet(4096));CHECK(active.connectionFailed);return true;};
 active.drain();CHECK(calls==1 && active.pendingSendBytes==1024*1024-4096 && active.sendQ.size()==255);
 Session reset;reset.pushSend(packet(20));reset.pushSend(packet(14,MsgType::INPUT));reset.pushSend(packet(21,MsgType::SEED));reset.pushSend(packet(19,MsgType::HASH));
 calls=0;transport=[&](const std::uint8_t*,std::size_t n){if(calls++==0){CHECK(n==20);reset.clear_round();CHECK(reset.pendingSendBytes==41&&reset.sendQ.size()==1);}return true;};
 reset.drain();CHECK(calls==2 && reset.pendingSendBytes==0 && reset.sendQ.empty());
 Session fair;for(unsigned i=0;i<100;++i)fair.pushSend(packet(10));calls=0;
 transport=[&](const std::uint8_t*,std::size_t){++calls;return true;};fair.drain();CHECK(calls==64 && fair.sendQ.size()==36 && fair.pendingSendBytes==360);
 fair.drain();CHECK(calls==100 && fair.pendingSendBytes==0);
 Session error;error.pushSend(packet(10));error.pushSend(packet(11));transport=[](const std::uint8_t*,std::size_t){return false;};error.drain();CHECK(error.quit&&error.connectionFailed&&error.pendingSendBytes==11&&error.sendQ.size()==1);
 Session count;for(unsigned i=0;i<4096;++i)count.pushSend(packet(7));CHECK(!count.quit);count.pushSend(packet(7));CHECK(count.quit&&count.pendingSendBytes==4096*7);
 Session zero;zero.pushSend({});CHECK(zero.quit&&zero.sendQ.empty());
 Session big;big.pushSend(packet(kMaxPayloadBytes+8));CHECK(big.quit&&big.pendingSendBytes==0);
 Session stopped;stopped.quit=true;stopped.pushSend(packet(7));CHECK(stopped.sendQ.empty());
#endif
 std::puts("actual send admission: byte/count caps, active charge, round filter, fairness, failure passed");
}
