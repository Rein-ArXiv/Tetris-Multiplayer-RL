#include "simulation/lines.h"
#include "simulation/round.h"
#include "src/clear_example.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"lines line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
static unsigned mask(const study_grid::Grid& b,int r) {
    unsigned m=0;for(int c=0;c<10;++c)if(!b.is_empty(r,c))m|=1u<<c;return m;
}
static int count(const study_grid::Grid& b) {
    int n=0;for(auto cell:b.cells())if(cell==study_grid::Cell::filled)++n;return n;
}
int main() {
    // All 2^20 choices of full rows. Every survivor has a distinct non-full
    // bit pattern. The oracle stores survivors in a separate array top-first.
    for(std::uint32_t pattern=0;pattern<(1u<<20);++pattern) {
        study_grid::Grid board;
        std::array<unsigned,20> kept{};int survivors=0,before=0;
        for(int r=0;r<20;++r) {
            const bool full=(pattern&(1u<<r))!=0;
            const unsigned bits=full?1023u:static_cast<unsigned>((r+1)*17);
            if(!full)kept[survivors++]=bits;
            for(int c=0;c<10;++c)if(bits&(1u<<c)) {
                CHECK(board.set(r,c,study_grid::Cell::filled));++before;
            }
            CHECK(study_lines::row_full(board,r)==full);
        }
        const int removed=study_lines::clear_full_rows(board);
        CHECK(removed==20-survivors);CHECK(count(board)==before-10*removed);
        for(int r=0;r<20;++r)CHECK(mask(board,r)==(r<removed?0u:kept[r-removed]));
        if(pattern%257==0) {
            const auto snapshot=board.cells();CHECK(study_lines::clear_full_rows(board)==0);
            CHECK(board.cells()==snapshot);
        }
    }
    study_grid::Grid board;
    for(int r:{-1,20,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
        CHECK(!study_lines::row_full(board,r));
    // Exercise every possible bit pattern for a single row, including empty.
    for(unsigned bits=0;bits<1024;++bits) {
        board.clear();for(int c=0;c<10;++c)if(bits&(1u<<c))CHECK(board.set(12,c,study_grid::Cell::filled));
        CHECK(study_lines::row_full(board,12)==(bits==1023));
        CHECK(study_lines::clear_full_rows(board)==(bits==1023?1:0));
        CHECK(mask(board,12)==(bits==1023?0:bits));
    }
    for(int k=0;k<7;++k) {
        auto kind=study_catalog::definitions[k].kind;
        auto round=study_round::Round::create(make_clear_board(kind),kind);
        const int before=count(round->board());
        for(int tick=1;tick<=570;++tick) {
            auto result=round->tick(0);CHECK(result!=study_round::Step::invalid);
            if(tick<570)CHECK(round->last_cleared()==0);
            else CHECK(result==study_round::Step::locked);
        }
        const int removed=kind==study_catalog::Kind::O?2:1;
        CHECK(round->last_cleared()==removed);CHECK(count(round->board())==before+4-10*removed);
        CHECK(!round->finished()&&round->active()->origin.row==0&&round->gravity().elapsed==0);
        CHECK(!round->board().is_empty(5+removed,0)&&!round->board().is_empty(12+removed,9));
        const auto snapshot=round->board().cells();CHECK(round->tick(99)==study_round::Step::invalid);
        CHECK(round->last_cleared()==removed&&round->board().cells()==snapshot);
        CHECK(round->tick(0)==study_round::Step::waiting);CHECK(round->last_cleared()==0);
    }
    // Clear first, spawn second: the old cells fill rows 0/1, then disappear.
    board.clear();for(int r=0;r<2;++r)for(int c=0;c<10;++c)if(c!=4&&c!=5)CHECK(board.set(r,c,study_grid::Cell::filled));
    CHECK(board.set(2,4,study_grid::Cell::filled));
    auto rescue=study_round::Round::create(board,study_catalog::Kind::O,1);
    CHECK(rescue&&!rescue->finished());CHECK(rescue->tick(0)==study_round::Step::locked);
    CHECK(rescue->last_cleared()==2&&!rescue->finished());CHECK(count(rescue->board())==1);
    CHECK(!rescue->board().is_empty(2,4));
    // Empty rows are surviving rows too; do not settle individual columns.
    board.clear();CHECK(board.set(10,2,study_grid::Cell::filled));
    for(int c=0;c<10;++c)CHECK(board.set(19,c,study_grid::Cell::filled));
    CHECK(study_lines::clear_full_rows(board)==1);CHECK(!board.is_empty(11,2)&&board.is_empty(19,2));
    std::puts("1048576 full-row patterns, 1024 row masks, seven integrated clears, stable order, conservation and spawn ordering passed");
}
