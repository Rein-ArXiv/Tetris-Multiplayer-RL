#include "src/sim_game.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current spin line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
static SimGame tgame(int rows) {
    unsigned seed=1;while(SimGame(seed).CurrentBlockId()!=6){++seed;CHECK(seed<1000);}
    SimGame g(seed);auto& b=const_cast<int(&)[20][10]>(g.Grid());
    for(auto& row:b)for(auto& cell:row)cell=0;
    b[17][3]=b[19][3]=b[19][5]=1;
    if(rows>=1)for(int c=0;c<10;++c)if(c!=4)b[19][c]=1;
    if(rows>=2)for(int c=0;c<10;++c)if(c!=4&&c!=5)b[18][c]=1;
    return g;
}
int main() {
    for(int rows=0;rows<=2;++rows) {
        auto g=tgame(rows);g.SubmitInput(INPUT_ROTATE|INPUT_DROP);
        CHECK(g.lastTSpinLines==rows&&g.lastLinesCleared==rows);
        CHECK(g.Score()==400*(rows+1)&&g.AttackLinesSent()==2*rows);
        auto placement=tgame(rows);CHECK(placement.ApplyPlacement(3,1)==rows);
        CHECK(placement.lastTSpinLines==-1); // endpoint does not establish history
    }
    auto down=tgame(1);down.SubmitInput(INPUT_ROTATE);down.MoveBlockDown();down.SubmitInput(INPUT_DROP);
    CHECK(down.lastTSpinLines==-1&&down.Score()==100);
    auto move=tgame(1);move.SubmitInput(INPUT_ROTATE);move.SubmitInput(INPUT_LEFT);move.SubmitInput(INPUT_RIGHT);move.SubmitInput(INPUT_DROP);
    CHECK(move.lastTSpinLines==-1);
    for(auto rejected:{INPUT_LEFT,INPUT_ROTATE}) {
        auto g=tgame(1);g.SubmitInput(INPUT_ROTATE);
        auto& b=const_cast<int(&)[20][10]>(g.Grid());b[1][3]=1;
        const auto hash=g.StateHash();g.SubmitInput(rejected);CHECK(g.StateHash()==hash);
        g.SubmitInput(INPUT_DROP);CHECK(g.lastTSpinLines==1);
    }
    auto plain=tgame(1),rotated=plain;
    for(int i=0;i<4;++i)rotated.SubmitInput(INPUT_ROTATE);
    auto a=plain.StateHashBreakdown(),b=rotated.StateHashBreakdown();
    CHECK(a.grid==b.grid&&a.currentBlock==b.currentBlock&&a.rng==b.rng);
    CHECK(a.scoreFlags!=b.scoreFlags&&plain.StateHash()!=rotated.StateHash());
    // Public placement input must reject extreme columns without arithmetic UB.
    auto invalid=tgame(1);const auto original=invalid.StateHash();
    CHECK(invalid.ApplyPlacement(std::numeric_limits<int>::min(),0)==-1);
    CHECK(invalid.ApplyPlacement(std::numeric_limits<int>::max(),0)==-1);
    CHECK(invalid.StateHash()==original);
    for(unsigned seed=1;seed<=32;++seed)for(int rot=0;rot<4;++rot)
    for(int col:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
        SimGame g(seed);auto before=g.StateHash();CHECK(g.ApplyPlacement(col,rot)==-1);CHECK(g.StateHash()==before);
    }
    unsigned iseed=1;while(SimGame(iseed).CurrentBlockId()!=3){++iseed;CHECK(iseed<1000);}
    SimGame left_edge(iseed);CHECK(left_edge.ApplyPlacement(-2,1)==0);
    for(int r=16;r<20;++r)CHECK(left_edge.Grid()[r][0]==3);
    std::puts("Current T-spin 0/1/2, hard-drop preservation, successful-move invalidation, rejected-input preservation, placement exclusion and history hash passed");
}
