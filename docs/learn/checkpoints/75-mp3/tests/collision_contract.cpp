#include "simulation/catalog.h"
#include "simulation/collision.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"collision line %d: %s\n",__LINE__,#x);return 1; } } while(false)
using study_collision::Placement;
using study_movement::Result;
static bool same(const study_piece::Piece& a, const study_piece::Piece& b) {
    if(a.origin.row!=b.origin.row || a.origin.column!=b.origin.column)return false;
    for(std::size_t i=0;i<4;++i)if(a.local[i].row!=b.local[i].row || a.local[i].column!=b.local[i].column)return false;
    return true;
}
// Independent single-obstacle oracle. Inputs here are deliberately small.
static Placement oracle(const study_piece::Piece& p, int obstacle) {
    bool outside=false, occupied=false;
    for(auto c:p.local) {
        int r=p.origin.row+c.row, col=p.origin.column+c.column;
        if(r<0 || r>=20 || col<0 || col>=10)outside=true;
        else if(obstacle>=0 && r==obstacle/10 && col==obstacle%10)occupied=true;
    }
    return outside?Placement::outside:occupied?Placement::occupied:Placement::clear;
}
int main() {
    int placements=0,moves=0;
    for(int obstacle=-1;obstacle<200;++obstacle) {
        study_grid::Grid board;
        if(obstacle>=0)CHECK(board.set(obstacle/10,obstacle%10,study_grid::Cell::filled));
        const auto before_board=board.cells();
        for(const auto& d:study_catalog::definitions)
        for(int row:{-1,0,18,19,20}) for(int col:{-1,0,1,3,6,7,8,9,10}) {
            auto base=*study_catalog::make_piece(d.kind);base.origin={row,col};
            const auto placement=oracle(base,obstacle);
            CHECK(study_collision::classify(board,base)==placement);++placements;
            for(int direction:{-1,0,1}) {
                auto current=base, expected=base;
                Result result=Result::invalid;
                if(placement==Placement::clear) {
                    if(direction==0)result=Result::idle;
                    else {
                        expected.origin.column+=direction;
                        result=oracle(expected,obstacle)==Placement::clear?Result::moved:Result::blocked;
                        if(result!=Result::moved)expected=base;
                    }
                }
                CHECK(study_collision::try_shift(board,current,direction)==result);
                CHECK(same(current,expected));++moves;
            }
        }
        CHECK(board.cells()==before_board);
    }
    study_grid::Grid board;
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);
    // Empty corner of the bounding box is not an occupied piece cell.
    CHECK(board.set(0,3,study_grid::Cell::filled));
    CHECK(study_collision::classify(board,piece)==Placement::clear);
    // Candidate overlaps an obstacle; original remains entirely unchanged.
    CHECK(board.set(1,2,study_grid::Cell::filled));
    const auto before=piece;
    CHECK(study_collision::try_shift(board,piece,-1)==Result::blocked);
    CHECK(same(before,piece));
    // Current overlap is invalid even for idle; not recovery from a bad state.
    CHECK(board.set(0,4,study_grid::Cell::filled));
    CHECK(study_collision::try_shift(board,piece,0)==Result::invalid);
    CHECK(same(before,piece));
    // Outside outranks occupied for all 24 orders.
    piece.origin={-1,3};board.clear();CHECK(board.set(0,3,study_grid::Cell::filled));
    const auto local=piece.local;int order[]={0,1,2,3};
    do {
        for(int i=0;i<4;++i)piece.local[i]=local[order[i]];
        CHECK(study_collision::classify(board,piece)==Placement::outside);
    } while(std::next_permutation(order,order+4));
    piece=*study_catalog::make_piece(study_catalog::Kind::T);piece.origin.row=INT_MAX;
    CHECK(study_collision::classify(board,piece)==Placement::unrepresentable);
    board.clear();piece=*study_catalog::make_piece(study_catalog::Kind::T);
    for(int direction:{INT_MIN,-2,2,INT_MAX}) {
        CHECK(study_collision::try_shift(board,piece,direction)==Result::invalid);
        CHECK(same(piece,before));
    }
    // classify is a destination query, not a swept path test.
    CHECK(board.set(1,3,study_grid::Cell::filled));
    piece.origin.column=0;CHECK(study_collision::classify(board,piece)==Placement::clear);
    auto destination=piece;destination.origin.column=4;
    CHECK(study_collision::classify(board,destination)==Placement::clear);
    CHECK(study_collision::try_shift(board,piece,1)==Result::blocked);
    CHECK(piece.origin.column==0);
    board.clear();CHECK(board.set(1,2,study_grid::Cell::filled));
    piece.origin.column=3;CHECK(study_collision::classify(board,piece)==Placement::clear);
    piece.origin.column=-100;
    for(auto& c:piece.local)c.column+=103;
    CHECK(study_collision::classify(board,piece)==Placement::clear);
    // Writing the active piece into the board produces self-collision.
    board.clear();piece=before;const auto cells=study_piece::to_board(piece);CHECK(cells);
    for(auto c:*cells)CHECK(board.set(c.row,c.column,study_grid::Cell::filled));
    CHECK(study_collision::classify(board,piece)==Placement::occupied);
    std::printf("collision: %d placements, %d moves; all obstacle cells, precedence, hollow shape, invalid/idle and preservation passed\n",placements,moves);
}
