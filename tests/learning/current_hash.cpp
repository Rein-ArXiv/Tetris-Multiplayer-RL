#include "src/sim_game.h"
#include "core/hash.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
struct HashProbe {
    using HashBreakdown=SimGame::HashBreakdown;
    SimGrid sim_grid;
    std::vector<SimBlock> blocks{SimIBlock(),SimJBlock(),SimLBlock(),SimOBlock(),SimSBlock(),SimTBlock(),SimZBlock()};
    XorShift64Star rng{1},garbageRng{2};
    SimBlock currentBlock=SimIBlock();
    std::vector<SimBlock> nextBlocks;
    int score=0,gravityCounterTicks=0,dropIntervalTicks=30,softDropCounterTicks=0;
    int totalLinesCleared=0,level=1,attackLinesSent=0,pendingGarbage=0;
    bool gameOver=false,lastMoveWasRotate=false;
    HashBreakdown StateHashBreakdown() const;
    uint64_t StateHash() const;
    uint64_t DiagnosticStateHashV2() const;
    uint64_t ComputeStateHash(bool) const;
};
#include "production_hash.inc"
int main() {
    HashProbe a,b=a;std::swap(b.blocks[0],b.blocks[1]);
    CHECK(a.StateHash()==b.StateHash()); // Legacy field omission, not a hash collision.
    CHECK(a.StateHashBreakdown().rng==b.StateHashBreakdown().rng);
    CHECK(a.DiagnosticStateHashV2()!=b.DiagnosticStateHashV2());
    b=a;b.blocks.pop_back();CHECK(a.DiagnosticStateHashV2()!=b.DiagnosticStateHashV2());
    b=a;b.softDropCounterTicks=2;CHECK(a.DiagnosticStateHashV2()!=b.DiagnosticStateHashV2());
    b=a;b.pendingGarbage=3;CHECK(a.DiagnosticStateHashV2()!=b.DiagnosticStateHashV2());
    for(int r=0;r<20;++r)for(int c=0;c<10;++c) {
        b=a;b.sim_grid.grid[r][c]=9;
        CHECK(a.StateHash()!=b.StateHash()&&a.DiagnosticStateHashV2()!=b.DiagnosticStateHashV2());
    }
    std::puts("Extracted actual hash methods: legacy bag omission reproduced; SIMH/2 bag order/count, timers and every board cell detected.");
}
