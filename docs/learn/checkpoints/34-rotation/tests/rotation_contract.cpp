#include "simulation/rotation.h"
#include "simulation/round.h"
#include "simulation/pending_controls.h"
#include "src/clear_example.h"
#include "tests/rotation_oracle.h"
#include <limits>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"rotation line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
static unsigned bits(const study_piece::Shape& shape) {
    unsigned result=0;for(auto cell:shape) {CHECK(cell.row>=0&&cell.row<4&&cell.column>=0&&cell.column<4);result|=1u<<(cell.row*4+cell.column);}return result;
}
static bool same(const study_piece::Piece& a,const study_piece::Piece& b) {
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
int main() {
    using study_rotation::Result;
    unsigned cases=0;
    for(int obstacle=-1;obstacle<200;++obstacle) {
        study_grid::Grid board;if(obstacle>=0)CHECK(board.set(obstacle/10,obstacle%10,study_grid::Cell::filled));
        const auto before_board=board.cells();
        for(int k=0;k<7;++k)for(int q=0;q<4;++q) {
            const auto kind=study_catalog::definitions[k].kind;
            auto local=study_rotation::shape_at(kind,q);CHECK(local&&bits(*local)==rotation_oracle::masks[k][q]);
            for(int row=-1;row<=20;++row)for(int col=-1;col<=10;++col) {
                study_piece::Piece piece{*local,{row,col}};const auto before=piece;int quarter=q;
                auto result=study_rotation::try_clockwise(board,piece,kind,quarter);
                const bool current=rotation_oracle::fits(k,q,row,col,obstacle);
                const bool next=rotation_oracle::fits(k,(q+1)%4,row,col,obstacle);
                CHECK(result==(!current?Result::invalid:next?Result::rotated:Result::blocked));
                if(current&&next) {CHECK(quarter==(q+1)%4);CHECK(bits(piece.local)==rotation_oracle::masks[k][quarter]);CHECK(piece.origin.row==row&&piece.origin.column==col);}
                else {CHECK(quarter==q);CHECK(same(piece,before));}
                CHECK(board.cells()==before_board);++cases;
            }
        }
    }
    study_grid::Grid empty;
    for(const auto& def:study_catalog::definitions) {
        auto piece=*study_catalog::make_piece(def.kind);piece.origin={6,3};const auto before=piece;int q=0;
        for(int i=0;i<4;++i)CHECK(study_rotation::try_clockwise(empty,piece,def.kind,q)==Result::rotated);
        CHECK(q==0&&same(piece,before));
    }
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);const auto original=piece;
    for(int bad:{-1,4,std::numeric_limits<int>::max()}) {
        int q=bad;CHECK(study_rotation::try_clockwise(empty,piece,study_catalog::Kind::T,q)==Result::invalid);CHECK(q==bad&&same(piece,original));
    }
    int q=0;CHECK(study_rotation::try_clockwise(empty,piece,static_cast<study_catalog::Kind>(0),q)==Result::invalid);
    CHECK(study_rotation::try_clockwise(empty,piece,study_catalog::Kind::I,q)==Result::invalid);
    piece.local[0]=piece.local[1];CHECK(study_rotation::try_clockwise(empty,piece,study_catalog::Kind::T,q)==Result::invalid);
    CHECK(!study_rotation_math::clockwise(original.local,{1,0})); // mixed parity, not a cell grid
    auto extreme=original.local;extreme[0].row=std::numeric_limits<int>::min();
    CHECK(!study_rotation_math::clockwise(extreme,{0,0})); // negated int minimum is out of int
    auto integral=study_rotation_math::clockwise(original.local,{0,0});CHECK(integral);
    // Canonical ordering is not a geometric restriction on incoming cell order.
    piece=original;std::swap(piece.local[0],piece.local[3]);q=0;
    CHECK(study_rotation::try_clockwise(empty,piece,study_catalog::Kind::T,q)==Result::rotated);CHECK(bits(piece.local)==0x262);
    // O changes state but not cells. Pending input survives until one consumption.
    auto round=study_round::Round::create(empty,study_catalog::Kind::O,100);
    CHECK(round->tick(0,true)==study_round::Step::changed&&round->quarter()==1);
    CHECK(study_rotation::same_cells(round->active()->local,study_catalog::definitions[3].cells));
    study_input::PendingControls pending;pending.capture(false,false,true);pending.capture(false,false,true);
    auto input=pending.consume();CHECK(input.clockwise&&input.horizontal==0);CHECK(!pending.consume().clockwise);
    // Translate first, rotate second: input can escape the would-be blocker.
    study_grid::Grid obstacle;CHECK(obstacle.set(2,4,study_grid::Cell::filled));
    round=study_round::Round::create(obstacle,study_catalog::Kind::T,100);
    CHECK(round->tick(0,true)==study_round::Step::waiting&&round->quarter()==0);
    CHECK(round->tick(1,true)==study_round::Step::changed&&round->quarter()==1);
    // A blocked floor rotation does not stop the due gravity/lock transition.
    round=study_round::Round::create(empty,study_catalog::Kind::T,1);
    for(int i=0;i<18;++i)CHECK(round->tick(0)==study_round::Step::changed);
    CHECK(round->tick(0,true)==study_round::Step::locked&&round->quarter()==0);
    // New spawn resets a successful previous orientation too.
    round=study_round::Round::create(make_clear_board(study_catalog::Kind::O),study_catalog::Kind::O,1);
    for(int i=0;i<17;++i)CHECK(round->tick(0)==study_round::Step::changed);
    CHECK(round->tick(0,true)==study_round::Step::changed&&round->quarter()==1);
    CHECK(round->tick(0)==study_round::Step::locked&&round->quarter()==0&&round->last_cleared()==2);
    std::printf("%u rotation placements, 28 masks, round-trip, parity/overflow, cached-state, input/order and spawn reset passed\n",cases);
}
