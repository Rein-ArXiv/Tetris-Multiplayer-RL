#include "src/sim_game.h"
#include "simulation/round.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK failed at %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
class ProductionGarbageProbe {
public:
    explicit ProductionGarbageProbe(std::uint64_t seed):rng(seed?seed:0xC0FFEE123456789ull),garbageRng((seed?seed:0xC0FFEE123456789ull)^0x9E3779B97F4A7C15ull){}
    void InsertGarbage(int rows);
    SimGrid sim_grid;
    bool gameOver=false;
    XorShift64Star rng,garbageRng;
};
#include "production_garbage.inc"
int main(){
    for(std::uint64_t seed=0;seed<1000;++seed){
        SimGame actual(seed);auto study=*study_round::Round::create_seeded(study_grid::Grid{},seed);
        actual.AddPendingGarbage(1);actual.AddPendingGarbage(2);CHECK(study.add_garbage(1)&&study.add_garbage(2));
        actual.SubmitInput(INPUT_DROP);CHECK(study.tick(0,false,false,true)==study_round::Step::locked);
        CHECK(actual.CurrentBlockId()==static_cast<int>(study.kind())&&actual.RngState()==study.source().seeded()->rng_state());
        for(int r=0;r<20;++r)for(int c=0;c<10;++c)CHECK((actual.Grid()[r][c]!=0)==(study.board().get(r,c)==study_grid::Cell::filled));
    }
    for(std::uint64_t seed : {std::uint64_t{0},std::uint64_t{1},study_session::garbage_tag,std::numeric_limits<std::uint64_t>::max()}){
        for(int count : {-1,0,1,2,3,19,20,21,std::numeric_limits<int>::max()}){
            ProductionGarbageProbe actual(seed);auto holes=study_holes::HoleSource::seeded(seed);
            CHECK(actual.garbageRng.getState()==*holes.rng_state());
            study_grid::Grid before;
            for(int r=0;r<20;++r)for(int c=0;c<10;++c)if((r*3+c*5)%11==0){actual.sim_grid.grid[r][c]=1;CHECK(before.set(r,c,study_grid::Cell::filled));}
            const auto piece_state=actual.rng.getState();
            actual.InsertGarbage(count);
            const int rows=count<=0?0:count>20?20:count;
            const int hole=rows?static_cast<int>(holes.next()):0;
            const auto expected=study_combat::insert(before,rows,hole);CHECK(expected);
            CHECK(actual.garbageRng.getState()==*holes.rng_state()&&actual.rng.getState()==piece_state);
            CHECK(actual.gameOver==expected->overflow);
            for(int r=0;r<20;++r)for(int c=0;c<10;++c)CHECK((actual.sim_grid.grid[r][c]!=0)==(expected->board.get(r,c)==study_grid::Cell::filled));
        }
    }
    std::puts("1000 actual SimGame one-lock garbage boards/piece streams; 36 extracted InsertGarbage boundary cases including XOR-zero, negative/zero/oversized rows and independent piece state passed");
}
