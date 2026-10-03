#include "simulation/state_hash.h"
#include <cinttypes>
#include <cstdio>
int main() {
    auto round=study_round::Round::create_seeded(study_grid::Grid{},1);
    if(!round)return 1;
    for(unsigned tick=0;tick<=2;++tick) {
        const auto bytes=study_hash::state_bytes(*round);
        const auto digest=bytes.digest();if(!digest)return 1;
        std::printf("LRND/1 tick=%u bytes=%zu row=%d gravity=%d hash=%016" PRIx64 "\n",
            tick,bytes.size(),round->active()->origin.row,round->gravity().elapsed,*digest);
        if(tick<2)(void)round->tick(0);
    }
    const auto bytes=study_hash::state_bytes(*round);
    std::printf("bytes=");for(std::size_t i=0;i<bytes.size();++i)std::printf("%02x",unsigned(bytes.data()[i]));std::puts("");
    // Deliberately truncated digest: pigeonhole principle guarantees a repeat.
    int seen[256];for(auto& n:seen)n=-1;
    for(unsigned n=0;n<=256;++n) {
        study_hash::Bytes<4> value;value.u32(n);const auto short_hash=*value.digest()&255u;
        if(seen[short_hash]>=0){std::printf("8-bit collision: %d and %u -> %02x\n",seen[short_hash],n,unsigned(short_hash));break;}
        seen[short_hash]=static_cast<int>(n);
    }
}
