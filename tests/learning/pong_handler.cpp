#include "net/framing.h"
#include "net/pong_window.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <utility>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"pong line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using namespace net;
std::int64_t clock_ms=0;
std::int64_t now_ms(){return clock_ms;}
std::atomic_bool ready{true};std::atomic<std::int64_t> lastPongMs{0};PongWindow pendingPongs_;
std::vector<std::vector<std::uint8_t>> sent;
void pushSend(std::vector<std::uint8_t>&& f){sent.push_back(std::move(f));}
void handle(const Frame& f){switch(f.type){
#include "pong_handler.inc"
 default:break;}}
Frame pong(std::uint64_t token){Frame f{MsgType::PONG,{}};le_write_u64(f.payload,token);return f;}
int main(){
 clock_ms=100;handle({MsgType::PONG,{}});CHECK(lastPongMs==0);
 handle(pong(99));CHECK(lastPongMs==0);
 pendingPongs_.remember(100);clock_ms=110;handle(pong(100));CHECK(lastPongMs==110);
 clock_ms=150;handle(pong(100));CHECK(lastPongMs==110);
 pendingPongs_.remember(200);clock_ms=199;handle(pong(200));CHECK(lastPongMs==110);
 clock_ms=10200;handle(pong(200));CHECK(lastPongMs==110);
 ready=false;pendingPongs_.remember(10300);clock_ms=10301;handle(pong(10300));CHECK(lastPongMs==110);
 ready=true;handle(pong(10300));CHECK(lastPongMs==10301);
 for(unsigned n=0;n<20;++n){Frame f{MsgType::PING,std::vector<std::uint8_t>(n,7)};auto count=sent.size();handle(f);CHECK(sent.size()==count+(n==8));}
 ready=false;handle({MsgType::PING,std::vector<std::uint8_t>(8,7)});CHECK(sent.size()==1);
 PongWindow q;for(unsigned i=1;i<=17;++i)q.remember(i);CHECK(!q.consume(1,20,10000));CHECK(q.consume(2,20,10000));CHECK(!q.consume(2,20,10000));
 CHECK(!q.remember(2));CHECK(!q.consume(2,20,10000));
 PongWindow zero;CHECK(zero.remember(0));CHECK(zero.consume(0,0,10000));CHECK(!zero.remember(0));
 CHECK(zero.remember(UINT64_MAX));CHECK(zero.consume(UINT64_MAX,UINT64_MAX,10000));CHECK(!zero.remember(UINT64_MAX));
 std::puts("actual PING/PONG handler: length/correlation/replay/age/readiness/window bounds passed");
}
