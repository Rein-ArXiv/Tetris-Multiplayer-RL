// Compile the actual Session::SendInput body with observable boundary adapters.
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdio>
#include "net/framing.h"
using namespace net;
static int clocks=0;
static int64_t now_ms(){++clocks;return 777;}
struct Session {
 std::atomic_bool connected{false},ready{false},quit{false};
 std::atomic<int64_t> lastMainActivityMs_{123};
 std::atomic<uint32_t> lastLocalTick{8};
 std::vector<std::vector<uint8_t>> queued;
 void pushSend(std::vector<uint8_t>&& x){queued.push_back(std::move(x));}
 void SendInput(uint32_t,uint8_t);
};
#include "input_ready.inc"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d\n",__LINE__);return 1;}}while(false)
int main(){
 for(unsigned bits=0;bits<8;++bits){Session s;s.connected=bits&1;s.ready=bits&2;s.quit=bits&4;clocks=0;s.SendInput(9,1);
  const bool allowed=bits==3;
  CHECK(s.queued.size()==(allowed?1u:0u));CHECK(clocks==(allowed?1:0));
  CHECK(s.lastMainActivityMs_==(allowed?777:123));CHECK(s.lastLocalTick==(allowed?9u:8u));
 }
 std::puts("actual SendInput: all 8 readiness/connection/quit combinations, no rejected side effects");
}
