#include "src/sim_game.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"score line %d: %s\n",__LINE__,#x);std::exit(1);} } while(false)
static unsigned seed_for(int id){for(unsigned seed=1;seed<1000;++seed)if(SimGame(seed).CurrentBlockId()==id)return seed;std::exit(2);}
static void prepare_vertical_i(SimGame& game,int rows){
    auto& board=const_cast<int(&)[20][10]>(game.Grid()); // test fixture on non-const owner
    for(int r=20-rows;r<20;++r)for(int c=0;c<10;++c)if(c!=5)board[r][c]=1;
}
int main(){
    const unsigned seed=seed_for(3);
    const int bases[]={0,100,300,600,1000};
    for(int level=1;level<=20;++level)for(int rows=0;rows<=4;++rows){
        SimGame game(seed);prepare_vertical_i(game,rows);
        game.level=level;game.totalLinesCleared=(level-1)*10+9;game.score=17;
        CHECK(game.ApplyPlacement(3,1)==rows);
        const int expected_level=std::min(20,1+((level-1)*10+9+rows)/10);
        CHECK(game.Score()==17+bases[rows]*level&&game.level==expected_level);
        CHECK(game.totalLinesCleared==(level-1)*10+9+rows);
        const auto hash=game.StateHash();for(int i=0;i<50;++i){CHECK(game.Score()==17+bases[rows]*level);CHECK(game.StateHash()==hash);}
    }
    for(int rows=1;rows<=4;++rows)for(int remaining:{0,1,50,1000}){
        SimGame game(seed);prepare_vertical_i(game,rows);game.level=20;
        game.totalLinesCleared=std::numeric_limits<int>::max()-1;
        game.score=std::numeric_limits<int>::max()-remaining;
        CHECK(game.ApplyPlacement(3,1)==rows); // actual rows, even if total saturates
        CHECK(game.Score()==std::numeric_limits<int>::max());
        CHECK(game.totalLinesCleared==std::numeric_limits<int>::max()&&game.level==20);
    }
    const int spin_bases[]={400,800,1200,1600};
    for(int level:{1,20})for(int rows=0;rows<=3;++rows)for(bool near_limit:{false,true}){
        SimGame spin(seed_for(6));spin.SubmitInput(INPUT_ROTATE);
        CHECK(spin.CurrentRotation()==1);
        auto& field=const_cast<int(&)[20][10]>(spin.Grid());
        field[0][3]=field[0][5]=field[2][3]=field[2][5]=1;
        for(int r=0;r<rows;++r)for(int c=0;c<10;++c)field[r][c]=1;
        for(auto cell:spin.CurrentBlock().GetCellPositions())field[cell.row][cell.column]=0;
        spin.level=level;spin.totalLinesCleared=(level-1)*10;
        spin.score=near_limit?std::numeric_limits<int>::max()-1:0;
        spin.MoveBlockDown();
        CHECK(spin.lastTSpinLines==rows&&spin.lastLinesCleared==rows);
        CHECK(spin.Score()==(near_limit?std::numeric_limits<int>::max():spin_bases[rows]*level));
    }
    // The final lock still scores even if the production pre-clear spawn check ends the game.
    SimGame ended(seed);
    auto& board=const_cast<int(&)[20][10]>(ended.Grid());
    for(int c=0;c<10;++c)if(c<3||c>6)board[1][c]=1;
    board[2][3]=1;ended.MoveBlockDown();
    CHECK(ended.IsGameOver()&&ended.Score()==100&&ended.totalLinesCleared==1);
    const auto hash=ended.StateHash();for(int n=0;n<50;++n){ended.Tick();ended.SubmitInput(INPUT_DROP);CHECK(ended.StateHash()==hash);}
    std::puts("score: 100 ordinary level/row cases, 16 saturation placements, 16 T-spin cases, actual clear return, ended final award and read-only queries passed");
}
