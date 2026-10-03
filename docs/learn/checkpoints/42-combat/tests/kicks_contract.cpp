#include "simulation/kicks.h"
#include "simulation/round.h"
#include "tests/rotation_oracle.h"
#include "tests/kicks_cases.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"kicks line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
static bool same(const study_piece::Piece& a,const study_piece::Piece& b){
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
// Independent explicit policy table, all offsets relative to the original origin.
constexpr int offsets[4][7][2]={
 {{0,0},{0,-1},{0,1},{0,-2},{0,2},{-1,0},{-2,0}},
 {{0,0},{0,1},{0,-1},{0,2},{0,-2},{-1,0},{-2,0}},
 {{0,0},{0,1},{0,-1},{0,2},{0,-2},{-1,0},{-2,0}},
 {{0,0},{0,-1},{0,1},{0,-2},{0,2},{-1,0},{-2,0}}
};
int main(){
    using study_rotation::Result;
    unsigned count=0,chosen[7]{};
    for(int obstacle=-1;obstacle<200;++obstacle){
        study_grid::Grid board;if(obstacle>=0)CHECK(board.set(obstacle/10,obstacle%10,study_grid::Cell::filled));
        const auto saved=board.cells();
        for(int k=0;k<7;++k)for(int q=0;q<4;++q)for(int row=-1;row<=20;++row)for(int col=-1;col<=10;++col){
            auto kind=study_catalog::definitions[k].kind;
            study_piece::Piece piece{*study_rotation::shape_at(kind,q),{row,col}};
            const auto before=piece;int phase=q;
            const bool valid=rotation_oracle::fits(k,q,row,col,obstacle);
            int expected=-1;
            if(valid)for(int i=0;i<(k==3?1:7);++i){
                if(rotation_oracle::fits(k,(q+1)%4,row+offsets[q][i][0],col+offsets[q][i][1],obstacle)){expected=i;break;}
            }
            const auto result=study_kicks::try_clockwise(board,piece,kind,phase);
            CHECK(result.result==(!valid?Result::invalid:expected<0?Result::blocked:Result::rotated));
            CHECK(result.candidate_index==expected);
            if(expected>=0){
                ++chosen[expected];CHECK(phase==(q+1)%4);
                unsigned mask=0;
                for(auto cell:piece.local){
                    CHECK(cell.row>=0&&cell.row<4&&cell.column>=0&&cell.column<4);
                    mask|=1u<<(cell.row*4+cell.column);
                }
                CHECK(mask==rotation_oracle::masks[k][phase]);
                CHECK(piece.origin.row==row+offsets[q][expected][0]&&piece.origin.column==col+offsets[q][expected][1]);
            }else{CHECK(phase==q&&same(piece,before));}
            CHECK(board.cells()==saved);++count;
        }
    }
    for(int i=0;i<7;++i)CHECK(chosen[i]>0);
    for(int n=0;n<7;++n){
        auto c=kicks_cases::make(n);auto piece=study_piece::Piece{*study_rotation::shape_at(c.kind,c.quarter),c.origin};
        int q=c.quarter;auto before=piece;const auto board=c.board.cells();
        auto out=study_kicks::try_clockwise(c.board,piece,c.kind,q);
        CHECK(out.candidate_index==c.expected_index);
        CHECK(out.result==(n==5?Result::blocked:Result::rotated));
        CHECK(piece.origin.row==c.expected_origin.row&&piece.origin.column==c.expected_origin.column);
        CHECK(q==(n==5?c.quarter:(c.quarter+1)%4));CHECK(c.board.cells()==board);
        if(n==5)CHECK(same(piece,before));
    }
    study_grid::Grid empty;
    auto piece=*study_catalog::make_piece(study_catalog::Kind::T);int q=0;
    piece.origin={18,3};
    for(int i=0;i<4;++i)CHECK(study_kicks::try_clockwise(empty,piece,study_catalog::Kind::T,q).result==Result::rotated);
    CHECK(q==0&&piece.origin.row==17); // Four orientations do not undo a kick translation.
    const auto before=piece;
    for(int bad:{-1,4,std::numeric_limits<int>::max()}){
        int phase=bad;auto out=study_kicks::try_clockwise(empty,piece,study_catalog::Kind::T,phase);
        CHECK(out.result==Result::invalid&&out.candidate_index==-1&&phase==bad&&same(piece,before));
    }
    q=0;CHECK(study_kicks::try_clockwise(empty,piece,static_cast<study_catalog::Kind>(0),q).result==Result::invalid);
    piece.local[0]=piece.local[1];CHECK(study_kicks::try_clockwise(empty,piece,study_catalog::Kind::T,q).result==Result::invalid);
    // Search arithmetic: an unrepresentable candidate must not prevent a later valid one.
    piece=*study_catalog::make_piece(study_catalog::Kind::T);piece.origin={5,3};
    study_kick_search::Policy list;list.count=2;list.offsets[0]={std::numeric_limits<int>::max(),0};list.offsets[1]={0,0};
    auto found=study_kick_search::first_clear(empty,piece,list);CHECK(found&&found->index==1&&same(found->piece,piece));
    list.count=0;CHECK(!study_kick_search::first_clear(empty,piece,list));list.count=8;CHECK(!study_kick_search::first_clear(empty,piece,list));
    // Different valid order, different gameplay result on the same board.
    auto c=kicks_cases::make(3);piece={*study_rotation::shape_at(c.kind,1),c.origin};
    list=*study_kicks::policy(c.kind,0);std::swap(list.offsets[1],list.offsets[2]);
    found=study_kick_search::first_clear(c.board,piece,list);CHECK(found&&found->index==1&&found->piece.origin.column==4);
    // Tick: a kick does not reset gravity. At the floor it can still lock this tick.
    auto round=study_round::Round::create(empty,study_catalog::Kind::T,1);
    for(int i=0;i<18;++i)CHECK(round->tick(0)==study_round::Step::changed);
    CHECK(round->tick(0,true)==study_round::Step::locked);
    CHECK(round->quarter()==0&&round->last_rotation_candidate()==5&&round->gravity().elapsed==0);
    const auto snapshot=round->board().cells();
    CHECK(round->tick(9,true)==study_round::Step::invalid&&round->last_rotation_candidate()==5&&round->board().cells()==snapshot);
    CHECK(round->tick(0)==study_round::Step::changed&&round->last_rotation_candidate()==-1);
    // Shift then rotate: with an obstacle at (2,4), direct rotation selects left,
    // while the same tick's right movement makes the zero-offset rotation fit.
    study_grid::Grid obstacle;CHECK(obstacle.set(2,4,study_grid::Cell::filled));
    round=study_round::Round::create(obstacle,study_catalog::Kind::T,100);
    CHECK(round->tick(0,true)==study_round::Step::changed&&round->last_rotation_candidate()==1&&round->active()->origin.column==2);
    round=study_round::Round::create(obstacle,study_catalog::Kind::T,100);
    CHECK(round->tick(1,true)==study_round::Step::changed&&round->last_rotation_candidate()==0&&round->active()->origin.column==4);
    std::printf("%u ordered-kick placements; all seven candidate indices exercised; fixtures, first-success, preservation, overflow, order and tick integration passed\n",count);
}
