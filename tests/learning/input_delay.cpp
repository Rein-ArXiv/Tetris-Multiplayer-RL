#include "net/input_pair.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}} while(0)
struct Session {
    uint32_t maximum=0;
    std::unordered_map<uint32_t,uint8_t> inputs;
    uint32_t maxRemoteTick() const { return maximum; }
    bool GetRemoteInput(uint32_t t,uint8_t& v) {
        auto it=inputs.find(t);if(it==inputs.end())return false;v=it->second;return true;
    }
};
struct Game {bool gameOver=false;void SubmitInput(uint8_t){}void Tick(){}};
int64_t limit(uint32_t localTickNext,uint32_t remote,uint8_t inputDelay) {
    Session session;session.maximum=remote;
#include "main_delay_policy.inc"
    return safeTick;
}
std::array<unsigned,8> trace(uint8_t inputDelay) {
    const unsigned arrived[]={0,1,2,2,2,5,6,7};
    Session session;
    std::unordered_map<uint32_t,uint8_t> localInputs;
    Game local,remote;Game* gameLocal=&local;Game* gameRemote=&remote;
    uint32_t simTick=0,received=0;
    std::array<unsigned,8> out{};
    for(uint32_t pulse=0;pulse<8;++pulse) {
        localInputs[pulse]=0;
        const uint32_t localTickNext=pulse+1;
        while(received<=arrived[pulse])session.inputs[received++]=0;
        session.maximum=arrived[pulse];
#include "main_delay_policy.inc"
#include "main_input_loop.inc"
            ++simTick;
        }
        out[pulse]=simTick;
    }
    return out;
}
int main() {
    CHECK(limit(0,0,2)==-3 && limit(0,0,255)==-256);
    CHECK(limit(1,UINT32_MAX,0)==0 && limit(UINT32_MAX,UINT32_MAX,255)==int64_t(UINT32_MAX)-256);
    const auto zero=trace(0),two=trace(2);
    std::printf("D0:");for(auto n:zero)std::printf(" %u",n);
    std::printf("\nD2:");for(auto n:two)std::printf(" %u",n);std::puts("");
    const std::array<unsigned,8> expected0{{1,2,3,3,3,6,7,8}},expected2{{0,0,1,2,3,4,5,6}};
    CHECK(zero==expected0 && two==expected2);
    std::puts("Actual main: local-clock delay absorbs two-pulse gap without reindexing");
}
