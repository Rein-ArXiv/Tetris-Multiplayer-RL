#include "simulation/round.h"
#include "tests/locking_oracle.h"
#include "src/spawn_example.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"ghost line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
static bool same(const study_piece::Piece& a,const study_piece::Piece& b){
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(int i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
static void same_round(const study_round::Round& a,const study_round::Round& b){
    CHECK(a.board().cells()==b.board().cells());CHECK(bool(a.active())==bool(b.active()));
    if(a.active())CHECK(same(*a.active(),*b.active()));
    CHECK(a.gravity().elapsed==b.gravity().elapsed&&a.gravity().interval==b.gravity().interval);
    CHECK(a.score()==b.score()&&a.total_lines()==b.total_lines()&&a.level()==b.level());
    CHECK(a.last_awarded()==b.last_awarded()&&a.last_cleared()==b.last_cleared());
    CHECK(a.kind()==b.kind()&&a.quarter()==b.quarter()&&a.source_cursor()==b.source_cursor());
    CHECK(a.end_reason()==b.end_reason()&&a.last_rotation_candidate()==b.last_rotation_candidate());
    for(std::size_t n=0;n<3;++n)CHECK(a.next().peek(n)==b.next().peek(n));
}
static constexpr study_catalog::Kind kinds[]={study_catalog::Kind::I,study_catalog::Kind::J,
    study_catalog::Kind::L,study_catalog::Kind::O,study_catalog::Kind::S,study_catalog::Kind::T,study_catalog::Kind::Z};
int main(){
    unsigned valid=0,invalid=0;
    for(int obstacle=-1;obstacle<200;++obstacle){
        study_grid::Grid grid;locking_oracle::Board oracle{};
        if(obstacle>=0){CHECK(grid.set(obstacle/10,obstacle%10,study_grid::Cell::filled));oracle[obstacle]=true;}
        for(int kind=0;kind<7;++kind)for(int row=0;row<20;++row)for(int col=-1;col<=10;++col){
            auto piece=*study_catalog::make_piece(kinds[kind]);
            piece.origin={row,col};const auto before=piece;const auto cells=grid.cells();
            const auto landing=study_ghost::project(grid,piece);
            CHECK(same(before,piece)&&cells==grid.cells());
            if(!locking_oracle::fits(oracle,kind,row,col)){CHECK(!landing);++invalid;continue;}
            ++valid;int expected=row;while(locking_oracle::fits(oracle,kind,expected+1,col))++expected;
            CHECK(landing&&landing->piece.origin.row==expected&&landing->distance==expected-row);
            auto wanted=piece;wanted.origin.row=expected;CHECK(same(wanted,landing->piece));
            CHECK(study_collision::classify(grid,landing->piece)==study_collision::Placement::clear);
            auto next=landing->piece;CHECK(study_gravity::try_down(grid,next)==study_movement::Result::blocked);
        }
    }
    // Final empty placement below an obstacle is not reachable through it.
    study_grid::Grid roof;CHECK(roof.set(10,4,study_grid::Cell::filled));
    auto t=*study_catalog::make_piece(study_catalog::Kind::T);auto far=t;far.origin.row=18;
    CHECK(study_collision::classify(roof,far)==study_collision::Placement::clear);
    auto landing=study_ghost::project(roof,t);CHECK(landing&&landing->piece.origin.row==8&&landing->distance==8);
    auto bad=t;bad.origin.row=std::numeric_limits<int>::max();CHECK(!study_ghost::project(roof,bad));
    // Compensating origin/local coordinates still map to valid cells.
    auto compensated=t;compensated.origin.row=-1000;
    for(auto& cell:compensated.local)cell.row+=1000;
    const auto equivalent=study_ghost::project(roof,compensated);
    CHECK(equivalent&&equivalent->distance==8&&equivalent->piece.origin.row==-992);
    auto round=*study_round::Round::create(roof,study_catalog::Kind::T,1);
    for(int tick=0;tick<100&&!round.finished();++tick){
        const auto before=round;
        for(int n=0;n<20;++n){const auto hint=round.ghost();CHECK(hint);same_round(before,round);}
        const auto step=round.tick(tick%7==0?1:0,tick%11==0);CHECK(step!=study_round::Step::invalid);
    }
    study_grid::Grid blocked;CHECK(blocked.set(0,4,study_grid::Cell::filled));
    auto ended=study_round::Round::create(blocked,study_catalog::Kind::T);CHECK(ended&&ended->finished());
    const auto before=*ended;CHECK(!ended->ghost());same_round(before,*ended);
    std::printf("ghost: %u valid + %u invalid placements, path barrier, zero distance, bounded arithmetic and read-only round queries passed\n",valid,invalid);
}
