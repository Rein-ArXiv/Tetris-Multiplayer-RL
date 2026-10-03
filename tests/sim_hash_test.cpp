#include "src/sim_game.h"
#include "core/hash.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main() {
    const unsigned char bytes[]{0x78,0x56,0x34,0x12};
    CHECK(fnv1a64_value(uint32_t{0x12345678})==fnv1a64(bytes,4));
    const unsigned char minus[]{0xfe,0xff,0xff,0xff};
    CHECK(fnv1a64_value(int32_t{-2})==fnv1a64(minus,4));
    CHECK(SimGame(1).StateHash()==0xfb7249998a4f8ed6ull);
    CHECK(SimGame(0).StateHash()==SimGame(0xC0FFEE123456789ull).StateHash());
    for(uint64_t seed=0;seed<1000;++seed) {
        SimGame a(seed),b=a;
        for(int tick=0;tick<70;++tick) {
            const auto h=a.DiagnosticStateHashV2();
            // Audio/visual consumers can clear these without changing rules.
            a.rotateSoundEvent=!a.rotateSoundEvent;a.hardDropEvent=!a.hardDropEvent;
            CHECK(a.DiagnosticStateHashV2()==h);
            const uint8_t input=tick%13==0?INPUT_DROP:tick%3==0?INPUT_ROTATE:INPUT_LEFT;
            a.SubmitInput(input);b.SubmitInput(input);a.Tick();b.Tick();
            CHECK(a.StateHash()==b.StateHash());
            CHECK(a.DiagnosticStateHashV2()==b.DiagnosticStateHashV2());
            const auto x=a.StateHashBreakdown(),y=b.StateHashBreakdown();
            CHECK(x.grid==y.grid&&x.currentBlock==y.currentBlock&&x.nextBlock==y.nextBlock&&x.rng==y.rng&&x.scoreFlags==y.scoreFlags&&x.combat==y.combat);
        }
    }
    std::puts("Fixed scalar byte vectors, legacy value, diagnostic copies and presentation exclusions passed.");
}
