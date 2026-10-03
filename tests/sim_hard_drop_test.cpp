#include "src/sim_game.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current hard drop line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using Board=std::array<int,200>;
static Board board_of(const SimGame& game) {
    Board b{};for(int r=0;r<20;++r)for(int c=0;c<10;++c)b[r*10+c]=game.Grid()[r][c];return b;
}
static bool fits(const Board& b,const std::vector<Position>& cells,int down) {
    for(auto p:cells) {
        const int r=p.row+down,c=p.column;
        if(r<0||r>=20||c<0||c>=10)return false;
        if(b[r*10+c]!=0&&b[r*10+c]!=8)return false;
    }
    return true;
}
static int landing_distance(const SimGame& g) {
    const auto b=board_of(g);const auto cells=g.CurrentBlock().GetCellPositions();
    CHECK(fits(b,cells,0));int down=0;
    while(fits(b,cells,down+1)){++down;CHECK(down<20);}return down;
}
int main() {
    unsigned locks=0,zero=0;
    for(unsigned seed=1;seed<=64;++seed) {
        SimGame g(seed);
        for(int step=0;step<40&&!g.IsGameOver();++step) {
            g.SubmitInput((step%2?INPUT_LEFT:INPUT_RIGHT)|(step%3?INPUT_ROTATE:0));
            auto b=board_of(g);const auto cells=g.CurrentBlock().GetCellPositions();
            const int down=landing_distance(g),next=g.NextBlockId();
            const auto original=g.StateHash();(void)g.GhostBlock();CHECK(g.StateHash()==original);
            for(auto p:cells)b[(p.row+down)*10+p.column]=g.CurrentBlockId();
            Board expected{};int write=19,cleared=0;
            for(int r=19;r>=0;--r) {
                bool full=true;for(int c=0;c<10;++c)full=full&&(b[r*10+c]!=0&&b[r*10+c]!=8);
                if(full){++cleared;continue;}
                for(int c=0;c<10;++c)expected[write*10+c]=b[r*10+c];
                --write;
            }
            g.dropSoundEvent=g.hardDropEvent=false;
            g.SubmitInput(INPUT_DROP);
            CHECK(board_of(g)==expected&&g.CurrentBlockId()==next&&g.CurrentRow()==0);
            CHECK(g.lastLinesCleared==cleared&&g.dropSoundEvent&&g.hardDropEvent);
            const auto committed=g.StateHash();g.dropSoundEvent=g.hardDropEvent=false;
            CHECK(g.StateHash()==committed);++locks;zero+=down==0;
        }
    }
    // A valid resting piece can be committed with no travel and no row-clear score.
    SimGame resting(1);const int d=landing_distance(resting);
    for(int n=0;n<d;++n)resting.MoveBlockDown();
    CHECK(landing_distance(resting)==0);const auto before=board_of(resting);
    resting.SubmitInput(INPUT_DROP);CHECK(board_of(resting)!=before&&resting.score==0);
    CHECK(resting.dropSoundEvent&&resting.hardDropEvent);
    // Actual production order: DOWN may lock, then DROP acts on the next piece.
    SimGame combined(1);const int distance=landing_distance(combined);
    for(int n=0;n<distance;++n)combined.MoveBlockDown();
    SimGame sequential=combined;
    combined.SubmitInput(INPUT_DOWN|INPUT_DROP);
    sequential.SubmitInput(INPUT_DOWN);sequential.SubmitInput(INPUT_DROP);
    CHECK(board_of(combined)==board_of(sequential));
    CHECK(combined.CurrentBlockId()==sequential.CurrentBlockId());
    unsigned cells=0;for(int value:board_of(combined))cells+=value!=0;CHECK(cells==8);
    // Finished game: rejected commands do not consume presentation reports either.
    resting.gameOver=true;const auto ended=resting.StateHash();
    const auto ended_board=board_of(resting);resting.SubmitInput(INPUT_DROP);
    CHECK(resting.StateHash()==ended&&board_of(resting)==ended_board);
    CHECK(resting.dropSoundEvent&&resting.hardDropEvent);
    std::printf("%u hard-drop locks (%u zero-travel), independent board/clear, flags outside hash, stopped and compound production order passed\n",locks,zero);
}
