#include "simulation/round.h"
#include "src/spin_example.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"history line %d: %s\n",__LINE__,#x);std::exit(1);} } while(false)
using study_catalog::Kind;
using study_grid::Cell;
using study_round::Round;
using study_round::Step;
int main() {
    // Exhaust every corner mask at every valid T pose/orientation. Independent
    // oracle counts physical border plus explicitly placed corner markers.
    unsigned poses=0;
    for(int q=0;q<4;++q) for(int row=-2;row<20;++row) for(int col=-2;col<10;++col)
    for(unsigned mask=0;mask<16;++mask) {
        study_piece::Piece p{*study_rotation::shape_at(Kind::T,q),{row,col}};
        study_grid::Grid b;
        if(study_collision::classify(b,p)!=study_collision::Placement::clear)continue;
        int blocked=0,bit=0;
        for(int dr:{0,2}) for(int dc:{0,2}) {
            int r=row+dr,c=col+dc;
            const bool outside=r<0||r>=20||c<0||c>=10;
            bool filled=(mask&(1u<<bit++))!=0;
            if(outside||filled)++blocked;
            if(!outside&&filled)CHECK(b.set(r,c,Cell::filled));
        }
        const auto before=b.cells();
        auto spin=study_spin::classify(b,p,Kind::T,q,true);
        CHECK(spin&&*spin==(blocked>=3));
        auto no_history=study_spin::classify(b,p,Kind::T,q,false);
        CHECK(no_history&&!*no_history&&b.cells()==before);++poses;
    }
    study_grid::Grid empty;
    auto bad=*study_catalog::make_piece(Kind::T);bad.origin.row=std::numeric_limits<int>::max();
    CHECK(!study_spin::classify(empty,bad,Kind::T,0,true));
    for(auto k:{Kind::I,Kind::J,Kind::L,Kind::O,Kind::S,Kind::Z}) {
        auto p=*study_catalog::make_piece(k);auto s=study_spin::classify(empty,p,k,0,true);CHECK(s&&!*s);
    }
    auto p=*study_catalog::make_piece(Kind::T);CHECK(!study_spin::classify(empty,p,Kind::T,1,true));
    CHECK(empty.set(0,4,Cell::filled));CHECK(!study_spin::classify(empty,p,Kind::T,0,true));
    const std::uint64_t counts[]={0,1,2,3,4,7,8,9,100,std::numeric_limits<std::uint64_t>::max()};
    unsigned rewards=0;
    for(auto before:counts)for(int rows=-1;rows<=5;++rows)for(bool spin:{false,true}) {
        auto r=study_history::reward({before},rows,spin);
        CHECK(bool(r)==(rows>=0&&rows<=(spin?3:4)));
        if(!r)continue;
        const auto n=rows==0?0:before==UINT64_MAX?before:before+1;
        const int tableNormal[]={0,0,1,2,4},tableSpin[]={0,2,4,6};
        int bonus=0;if(n>=9)bonus=4;else if(n>=7)bonus=3;else if(n>=5)bonus=2;else if(n>=3)bonus=1;
        CHECK(r->chain.clears==n&&r->attack==(spin?tableSpin[rows]:tableNormal[rows])+bonus);++rewards;
    }
    for(unsigned level=0;level<=21;++level)for(int rows=-1;rows<=4;++rows) {
        auto n=study_score::spin_points(rows,level);CHECK(bool(n)==(rows>=0&&rows<=3&&level>=1&&level<=20));
        if(n)CHECK(*n==static_cast<unsigned>(400*(rows+1))*level);
    }
    auto boundary=study_score::award({100,19},2,true);
    CHECK(boundary&&boundary->points==2500&&boundary->lines==21&&study_score::level(*boundary)==3);
    auto saturated=study_score::award({UINT64_MAX-1,UINT64_MAX-1},2,true);
    CHECK(saturated&&saturated->points==UINT64_MAX&&saturated->lines==UINT64_MAX);
    CHECK(!study_score::award({},4,true));
    for(int rows=0;rows<=2;++rows) {
        auto r=*Round::create(make_spin_board(rows),Kind::T);
        CHECK(r.tick(0,true,false,true)==Step::locked);
        CHECK(r.last_t_spin_lines()==rows&&r.score()==static_cast<unsigned>(400*(rows+1)));
        CHECK(r.attack_sent()==static_cast<unsigned>(2*rows));
        CHECK(r.clear_streak()==(rows?1u:0u)&&!r.rotation_ready());
        CHECK(r.tick(0)==Step::waiting&&r.last_t_spin_lines()==-1);
    }
    // Idle ticks preserve eligibility; successful down clears it. Requests alone
    // are insufficient, and new spawn resets it even after a scoring T-spin.
    auto held=*Round::create(make_spin_board(1),Kind::T,30);
    CHECK(held.tick(0,true)==Step::changed&&held.rotation_ready());
    auto unchanged=held;CHECK(held.tick(2)==Step::invalid);
    CHECK(held.rotation_ready()&&held.board().cells()==unchanged.board().cells());
    CHECK(held.gravity().elapsed==unchanged.gravity().elapsed&&held.quarter()==unchanged.quarter());
    CHECK(held.tick(0)==Step::waiting&&held.rotation_ready());
    auto soft=held;CHECK(soft.tick(0,false,true)==Step::changed&&!soft.rotation_ready());
    CHECK(soft.tick(0,false,false,true)==Step::locked&&soft.last_t_spin_lines()==-1);
    CHECK(held.tick(0,false,false,true)==Step::locked&&held.last_t_spin_lines()==1);
    auto gravity=*Round::create(make_spin_board(1),Kind::T,1);
    CHECK(gravity.tick(0,true)==Step::changed&&!gravity.rotation_ready());
    CHECK(gravity.tick(0,false,false,true)==Step::locked&&gravity.last_t_spin_lines()==-1);
    auto horizontal=*Round::create(make_spin_board(1),Kind::T);
    CHECK(horizontal.tick(0,true)==Step::changed&&horizontal.rotation_ready());
    CHECK(horizontal.tick(-1)==Step::changed&&!horizontal.rotation_ready());
    CHECK(horizontal.tick(1)==Step::changed);
    CHECK(horizontal.tick(0,false,false,true)==Step::locked&&horizontal.last_t_spin_lines()==-1);
    // Three consecutive O doubles, then a zero-line lock. Idle does not end chain.
    study_grid::Grid stack;for(int y=14;y<20;++y)for(int x=0;x<10;++x)if(x!=4&&x!=5)CHECK(stack.set(y,x,Cell::filled));
    auto combo=*Round::create(stack,Kind::O);
    for(unsigned n=1;n<=3;++n){CHECK(combo.tick(0,false,false,true)==Step::locked);CHECK(combo.clear_streak()==n);CHECK(combo.tick(0)==Step::waiting&&combo.clear_streak()==n);}
    CHECK(combo.attack_sent()==4&&combo.score()==900);
    CHECK(combo.tick(0,false,false,true)==Step::locked&&combo.clear_streak()==0);
    unsigned rejected=0,kicked=0;
    for(unsigned seed=1;seed<=300;++seed) {
        study_grid::Grid b;unsigned state=seed;
        for(int y=0;y<7;++y)for(int x=1;x<10;++x) {
            state=state*1664525u+1013904223u;
            if((state>>28)<5 && !(y==0&&x==4) && !(y==1&&x>=3&&x<=5))
                CHECK(b.set(y,x,Cell::filled));
        }
        auto r=Round::create(b,Kind::T,10000);CHECK(r&&!r->finished());
        for(int i=0;i<12;++i) {
            bool before=r->rotation_ready();auto step=r->tick(0,true);CHECK(step!=Step::invalid);
            if(r->last_rotation_candidate()<0){CHECK(r->rotation_ready()==before);++rejected;}
            else {CHECK(r->rotation_ready());kicked+=r->last_rotation_candidate()>0;}
        }
    }
    CHECK(rejected>0&&kicked>0);
    std::printf("rejected rotations=%u accepted kicks=%u\n",rejected,kicked);
    std::printf("%u corner poses, %u reward transitions, history/score/chain integration passed\n",poses,rewards);
}
