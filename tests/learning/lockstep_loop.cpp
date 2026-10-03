#include "net/input_pair.h"
#include <cstdio>
struct Game {
    bool gameOver = false;
    bool finish = false;
    void SubmitInput(uint8_t) {}
    void Tick() { if (finish) gameOver = true; }
};
struct Session {
    bool GetRemoteInput(uint32_t, uint8_t& value) { value = 0; return true; }
};
uint32_t consume(bool missing, int finishes) {
    Session session;
    std::unordered_map<uint32_t,uint8_t> localInputs;
    if (!missing) for (uint32_t i=0;i<3;++i) localInputs[i]=0;
    uint32_t simTick=0;
    int64_t safeTick=2;
    Game local,remote;
    local.finish = finishes==1;
    remote.finish = finishes==2;
    Game* gameLocal=&local;
    Game* gameRemote=&remote;
    // Extracted from the actual main.cpp by check_learning_lockstep.py.
    // The fixture replaces game/session I/O, not the loop's presence/end guards.
#include "main_input_loop.inc"
        ++simTick;
    }
    return simTick;
}
int main() {
    if (consume(true,0)!=0 || consume(false,0)!=3 ||
        consume(false,1)!=1 || consume(false,2)!=1) return 1;
    std::puts("Actual main loop: missing local waits; either finished board stops catch-up");
}
