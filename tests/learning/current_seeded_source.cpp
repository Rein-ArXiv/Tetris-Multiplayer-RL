#include "src/sim_game.h"
#include "simulation/seeded_bag_source.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK failed at %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
// Compiled with production functions extracted by the verifier, unmodified except class qualifier.
class ProductionBagProbe {
public:
    explicit ProductionBagProbe(std::uint64_t seed):rng(seed?seed:0xC0FFEE123456789ull){blocks=GetAllBlocks();}
    SimBlock GetRandomBlock();
    std::vector<SimBlock> GetAllBlocks() const;
    std::vector<SimBlock> blocks;
    XorShift64Star rng;
};
#include "production_bag_functions.inc"
int main(){
    for(std::uint64_t seed=0;seed<1000;++seed){
        SimGame game(seed);study_next::SeededBagSource source(seed);
        CHECK(game.CurrentBlockId()==static_cast<int>(source.next()));
        for(const auto& block:game.NextBlocks())CHECK(block.id==static_cast<int>(source.next()));
        CHECK(game.RngState()==source.rng_state());
    }
    for(std::uint64_t seed=0;seed<100;++seed){
        ProductionBagProbe actual(seed);study_next::SeededBagSource source(seed);
        for(int n=0;n<1000;++n){
            CHECK(actual.GetRandomBlock().id==static_cast<int>(source.next()));
            CHECK(actual.rng.getState()==source.rng_state()&&actual.blocks.size()==source.bag().remaining());
            for(std::size_t i=0;i<actual.blocks.size();++i)CHECK(actual.blocks[i].id==static_cast<int>(*source.bag().at(i)));
        }
    }
    std::puts("1000 actual SimGame initializations and 100000 extracted production draws match SeededBagSource including zero seed, bag order and RNG state");
}
