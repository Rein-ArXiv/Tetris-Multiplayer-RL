#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Breakdown {uint64_t grid=0,currentBlock=0,nextBlock=0,rng=0,scoreFlags=0,combat=0;};
struct Sim {Breakdown StateHashBreakdown()const{return {};}};
struct Game {Sim sim;};
enum class AppMode {Net};
struct Session {bool GetLastRemoteHash(uint32_t& t,uint64_t& h){t=600;h=2;return true;}};
int main(){
    Session session;AppMode app=AppMode::Net;
    Game local,remote;Game*gameLocal=&local;Game*gameRemote=&remote;
    uint32_t simTick=599;
#include "main_hash_storage.inc"
    auto compare=[&]{
#include "main_hash_compare.inc"
    };
    compare();CHECK(lastRemoteHashSeenTick==0 && !desyncDetected);
    localHashRing[1]={600,1,true};simTick=601;
    compare();CHECK(lastRemoteHashSeenTick==600&&desyncDetected&&desyncTick==600);
    desyncDetected=false;compare();CHECK(!desyncDetected);
#ifdef CHECK_RESET
    lastHashSentTick=600;resetHashComparison();
    CHECK(lastHashSentTick==UINT32_MAX&&lastRemoteHashSeenTick==0&&!desyncDetected&&desyncTick==0);
    for(const auto&slot:localHashRing)CHECK(!slot.valid);
#endif
    (void)lastHashSentTick;
    std::puts("Actual main: early remote hash retried; compared only once; round state reset");
}
