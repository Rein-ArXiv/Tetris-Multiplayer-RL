#include "net/input_pair.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main() {
 for(unsigned local_state=0;local_state<3;++local_state)
  for(unsigned remote_state=0;remote_state<3;++remote_state) {
   std::unordered_map<uint32_t,uint8_t> local;
   if(local_state)local[5]=local_state==1?0:16;
   unsigned calls=0;uint8_t li=77,ri=88;
   const bool ready=net::read_input_pair(local,5,[&](uint32_t tick,uint8_t& mask){++calls;CHECK(tick==5);mask=remote_state==1?0:8;return remote_state!=0;},li,ri);
   CHECK(ready==(local_state!=0&&remote_state!=0));
   if(ready)CHECK(li==(local_state==1?0:16)&&ri==(remote_state==1?0:8));
   else CHECK(li==77&&ri==88);
   CHECK(calls==(local_state?1u:0u));
  }
 // Highest received tick does not prove a hole is filled.
 std::unordered_map<uint32_t,uint8_t> local{{0,0},{2,16}},remote{{0,0},{2,8}};
 auto lookup=[&](uint32_t t,uint8_t& v){auto i=remote.find(t);if(i==remote.end())return false;v=i->second;return true;};
 uint8_t li=1,ri=2;CHECK(net::read_input_pair(local,0,lookup,li,ri)&&li==0&&ri==0);
 CHECK(!net::read_input_pair(local,1,lookup,li,ri));CHECK(local.size()==2&&remote.size()==2);
 std::puts("Input pair: absent/neutral/action matrix, output preservation and exact-tick holes passed");
}
