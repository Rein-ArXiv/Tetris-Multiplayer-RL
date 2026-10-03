#include "src/sim_game.h"
#include "core/saturating_count.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current combat line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using Board=std::array<int,200>;
static Board copy_board(const SimGame& g){Board b{};for(int r=0;r<20;++r)for(int c=0;c<10;++c)b[r*10+c]=g.Grid()[r][c];return b;}
static int first_hole(std::uint64_t seed){
    std::uint64_t x=(seed?seed:0xC0FFEE123456789ull)^0x9E3779B97F4A7C15ull;
    if(!x)x=88172645463393265ull;
    x^=x>>12;x^=x<<25;x^=x>>27;
    return static_cast<int>((x*2685821657736338717ull)%10u);
}
int main(){
    const int max=std::numeric_limits<int>::max();
    for(int a:{0,1,20,max-6,max-1,max})for(int b:{-1,0,1,6,20,max}){
        const std::int64_t sum=static_cast<std::int64_t>(a)+std::max(0,b);
        CHECK(saturating_add_count(a,b)==(sum>max?max:sum));
    }
    SimGame huge(1);huge.AddPendingGarbage(max);huge.AddPendingGarbage(1);
    CHECK(huge.PendingGarbage()==max);huge.AddPendingGarbage(-1);CHECK(huge.PendingGarbage()==max);
    huge.SubmitInput(INPUT_DROP);CHECK(huge.lastGarbageReceived==20&&huge.PendingGarbage()==0);
    CHECK(huge.gameOver&&huge.gameOverEvent&&huge.garbageSoundEvent);
    unsigned cases=0;
    for(unsigned seed=1;seed<=64;++seed)for(int rows=1;rows<=20;++rows)for(bool top:{false,true}){
        SimGame actual(seed),base(seed);
        for(SimGame* g:{&actual,&base}){
            auto& board=const_cast<int(&)[20][10]>(g->Grid());
            board[5][9]=1;board[12][0]=2;if(top)board[0][0]=3;
        }
        actual.AddPendingGarbage(rows);actual.SubmitInput(INPUT_DROP);base.SubmitInput(INPUT_DROP);
        CHECK(!base.gameOver);const auto before=copy_board(base);Board expected{};bool overflow=false;
        for(int i=0;i<rows*10;++i)if(before[i]!=0&&before[i]!=8)overflow=true;
        const int hole=first_hole(seed);
        for(int r=0;r<20;++r)for(int c=0;c<10;++c)
            expected[r*10+c]=r+rows<20?before[(r+rows)*10+c]:(c==hole?0:9);
        bool blocked=false;for(auto p:base.CurrentBlock().GetCellPositions())if(expected[p.row*10+p.column]!=0)blocked=true;
        CHECK(copy_board(actual)==expected&&actual.gameOver==(overflow||blocked));
        CHECK(actual.lastGarbageReceived==rows&&actual.PendingGarbage()==0&&actual.garbageSoundEvent);
        CHECK(actual.gameOverEvent==actual.gameOver);
        CHECK(actual.StateHashBreakdown().rng==base.StateHashBreakdown().rng); // piece source consumption unchanged
        ++cases;
    }
    // Original deliveries merge into one insertion and one hole sample.
    SimGame whole(17),parts(17);whole.AddPendingGarbage(3);parts.AddPendingGarbage(1);parts.AddPendingGarbage(2);
    CHECK(whole.StateHash()==parts.StateHash());whole.SubmitInput(INPUT_DROP);parts.SubmitInput(INPUT_DROP);
    CHECK(whole.StateHash()==parts.StateHash());
    const auto pending=whole.PendingGarbage();const auto h=whole.StateHash();(void)whole.PendingGarbage();CHECK(whole.PendingGarbage()==pending&&whole.StateHash()==h);
    // Ordinary clear-to-attack table through real locks, not just the helper.
    unsigned seed_i=1;while(SimGame(seed_i).CurrentBlockId()!=3)++seed_i;
    const int attacks[]={0,0,1,2,4};
    for(int rows=0;rows<=4;++rows){SimGame g(seed_i);auto& b=const_cast<int(&)[20][10]>(g.Grid());
        for(int r=20-rows;r<20;++r)for(int c=0;c<10;++c)if(c!=5)b[r][c]=1;
        CHECK(g.ApplyPlacement(3,1)==rows&&g.AttackLinesSent()==attacks[rows]);
    }
    std::printf("%u insertion boards, occupied overflow vs spawn, piece-RNG independence, merged delivery, actual capped report and saturating counts passed\n",cases);
}
