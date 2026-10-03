#include "src/sim_game.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"ghost line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
static bool fits(const SimGame& g,const SimBlock& b,int delta){
    for(auto p:b.GetCellPositions()) {
        const int r=p.row+delta,c=p.column;
        if(r<0||r>=20||c<0||c>=10)return false;
        if(g.Grid()[r][c]!=0&&g.Grid()[r][c]!=8)return false;
    }
    return true;
}
static void check(const SimGame& g){
    if(g.IsGameOver())return; // No landing hint is displayed after defeat.
    const auto hash=g.StateHash();
    const auto& active=g.CurrentBlock(); const auto& ghost=g.GhostBlock();
    int distance=0;CHECK(fits(g,active,0));
    while(fits(g,active,distance+1)){++distance;CHECK(distance<20);}
    CHECK(ghost.id==8&&ghost.rowOffset==active.rowOffset+distance);
    CHECK(ghost.columnOffset==active.columnOffset&&ghost.rotationState==active.rotationState);
    const auto ac=active.GetCellPositions(),gc=ghost.GetCellPositions();CHECK(ac.size()==gc.size());
    for(std::size_t i=0;i<ac.size();++i)CHECK(ac[i].row+distance==gc[i].row&&ac[i].column==gc[i].column);
    CHECK(g.StateHash()==hash);
}
int main(){
    for(unsigned seed=1;seed<=64;++seed){
        SimGame g(seed);check(g);
        for(int tick=0;tick<1200&&!g.IsGameOver();++tick){
            unsigned mask=(tick%13==0?INPUT_LEFT:0)|(tick%17==0?INPUT_RIGHT:0)|
                (tick%23==0?INPUT_ROTATE:0)|(tick%5==0?INPUT_DOWN:0);
            g.SubmitInput(mask);check(g);g.Tick();check(g);
        }
        SimGame natural(seed);int locks=0,last=natural.CurrentRow();
        for(int n=0;n<2500&&!natural.IsGameOver();++n){natural.Tick();check(natural);if(natural.CurrentRow()<last)++locks;last=natural.CurrentRow();}
        CHECK(locks>0);
        SimGame direct(seed);
        for(int n=0;n<50&&!direct.IsGameOver();++n){direct.AddPendingGarbage(n%3==0?1:0);direct.SubmitInput(INPUT_DROP);check(direct);}
        SimGame placement(seed);
        for(int n=0;n<30&&!placement.IsGameOver();++n){
            const auto choices=placement.LegalPlacements();if(choices.empty())break;
            const auto p=choices[static_cast<std::size_t>(n)%choices.size()];
            CHECK(placement.ApplyPlacement(p.col,p.rot)>=0);check(placement);
        }
        SimGame down(seed);for(int n=0;n<200&&!down.IsGameOver();++n){down.MoveBlockDown();check(down);}
    }
    std::puts("64 seeds: construction, masks, natural lock, direct down, drop/garbage and placements maintain landing cache; queries preserve hash");
}
