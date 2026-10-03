#include "simulation/round.h"
#include "src/gravity_example.h"
#include "tests/locking_oracle.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"locking line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
static bool same(const study_piece::Piece& a,const study_piece::Piece& b) {
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
int main() {
    using namespace study_locking;
    unsigned cases=0;
    for(int obstacle=-1;obstacle<200;++obstacle) {
        study_grid::Grid original;locking_oracle::Board expected{};
        if(obstacle>=0) { CHECK(original.set(obstacle/10,obstacle%10,study_grid::Cell::filled));expected[obstacle]=true; }
        for(int k=0;k<7;++k)for(int row=-1;row<=20;++row)for(int col=-1;col<=10;++col) {
            auto board=original;auto oracle=expected;
            auto piece=*study_catalog::make_piece(study_catalog::definitions[k].kind);
            piece.origin={row,col};const auto before=piece;
            const auto result=try_lock(board,piece);
            const bool valid=locking_oracle::fits(expected,k,row,col);
            const bool falling=valid&&locking_oracle::fits(expected,k,row+1,col);
            CHECK(result==(!valid?Result::invalid:falling?Result::not_grounded:Result::locked));
            if(valid&&!falling)locking_oracle::fill(oracle,k,row,col);
            for(int i=0;i<200;++i)CHECK((board.cells()[i]==study_grid::Cell::filled)==oracle[i]);
            CHECK(same(piece,before));++cases;
        }
    }
    // A duplicate cell must not turn a four-cell lock into three writes.
    study_grid::Grid board;auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    piece.origin={18,3};piece.local[3]=piece.local[2];const auto snapshot=board.cells();
    CHECK(try_lock(board,piece)==Result::invalid&&board.cells()==snapshot);
    // Permuting valid cells cannot change the four resulting writes.
    piece=*study_catalog::make_piece(study_catalog::Kind::T);piece.origin={18,3};
    std::array<int,4> order{0,1,2,3};unsigned permutations=0;
    do { auto perm=piece;for(int i=0;i<4;++i)perm.local[i]=piece.local[order[i]];
        auto copy=board;CHECK(try_lock(copy,perm)==Result::locked);unsigned count=0;
        for(auto c:copy.cells()) { if(c==study_grid::Cell::filled)++count; }
        CHECK(count==4);++permutations;
    } while(std::next_permutation(order.begin(),order.end()));CHECK(permutations==24);
    // Rebased local coordinates still denote a valid floor placement.
    auto rebased=piece;rebased.origin.row=0;for(auto& cell:rebased.local)cell.row+=18;
    CHECK(try_lock(board,rebased)==Result::locked);
    CHECK(try_lock(board,rebased)==Result::invalid); // Repeated lock would overwrite existing cells.
    auto extreme=piece;extreme.origin.row=std::numeric_limits<int>::max();
    const auto full_before=board.cells();CHECK(try_lock(board,extreme)==Result::invalid);CHECK(board.cells()==full_before);
    // Complete repeated-kind rounds, including spawn failure and terminal no-op.
    unsigned total_locks=0;
    for(int k=0;k<7;++k) {
        auto round=study_round::Round::create(make_gravity_board(),study_catalog::definitions[k].kind);
        CHECK(round&&!round->finished());locking_oracle::Pile oracle(k);int ticks=0;
        while(!round->finished()&&ticks<10000) {
            const auto result=round->tick(0);++ticks;CHECK(result!=study_round::Step::invalid);
            if(result==study_round::Step::locked||result==study_round::Step::game_over) {
                oracle.next_lock();++total_locks;
                CHECK(round->finished()==oracle.finished);CHECK(round->gravity().elapsed==0);
                for(int i=0;i<200;++i)CHECK((round->board().cells()[i]==study_grid::Cell::filled)==oracle.board[i]);
                if(round->active())CHECK(round->active()->origin.row==0&&round->active()->origin.column==oracle.column);
            }
        }
        CHECK(round->finished()&&oracle.finished);
        const auto final=round->board().cells();const int counter=round->gravity().elapsed;
        for(int direction:{-1,0,1,99})CHECK(round->tick(direction)==study_round::Step::stopped);
        CHECK(round->board().cells()==final&&round->gravity().elapsed==counter&&!round->active());
    }
    CHECK(!study_round::Round::create(board,static_cast<study_catalog::Kind>(0)));
    CHECK(!study_round::Round::create(board,study_catalog::Kind::T,0));
    study_grid::Grid blocked;CHECK(blocked.set(0,4,study_grid::Cell::filled));
    auto ended=study_round::Round::create(blocked,study_catalog::Kind::T);CHECK(ended&&ended->finished());
    CHECK(ended->board().cells()==blocked.cells());CHECK(ended->tick(0)==study_round::Step::stopped);
    auto active=study_round::Round::create(make_gravity_board(),study_catalog::Kind::T,1);
    const auto before=*active;CHECK(active->tick(2)==study_round::Step::invalid);
    CHECK(active->board().cells()==before.board().cells()&&same(*active->active(),*before.active()));
    CHECK(active->gravity().elapsed==before.gravity().elapsed);
    // Contact is not itself locking: row 8 is reached first, next failed fall locks.
    for(int i=0;i<8;++i)CHECK(active->tick(0)==study_round::Step::moved);
    CHECK(active->active()->origin.row==8&&active->board().cells()==make_gravity_board().cells());
    CHECK(active->tick(0)==study_round::Step::locked&&active->active()->origin.row==0);
    // At the due tick, a successful side step can escape support before falling.
    study_grid::Grid ledge;CHECK(ledge.set(2,3,study_grid::Cell::filled));
    auto escape=study_round::Round::create(ledge,study_catalog::Kind::T,1);
    CHECK(escape->tick(1)==study_round::Step::moved);CHECK(escape->active()->origin.row==1);
    CHECK(escape->board().cells()==ledge.cells());
    std::printf("%u lock placements, 24 permutations, %u complete-round locks and terminal/ownership contracts passed\n",cases,total_locks);
}
