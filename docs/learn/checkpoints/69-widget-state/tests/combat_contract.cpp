#include "simulation/combat.h"
#include "simulation/duel.h"
#include "src/clear_example.h"
#include "src/spawn_example.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"combat line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using namespace study_round;
using Kind=study_catalog::Kind;
static bool piece_equal(const study_piece::Piece& a,const study_piece::Piece& b){
    if(a.origin.row!=b.origin.row||a.origin.column!=b.origin.column)return false;
    for(unsigned i=0;i<4;++i)if(a.local[i].row!=b.local[i].row||a.local[i].column!=b.local[i].column)return false;
    return true;
}
static void same(const study_round::Round& a,const study_round::Round& b){
    CHECK(a.last_hard_drop_distance()==b.last_hard_drop_distance());
    CHECK(a.attack_sent()==b.attack_sent()&&a.pending_garbage()==b.pending_garbage());
    CHECK(a.last_garbage()==b.last_garbage()&&a.hole_cursor()==b.hole_cursor());
    CHECK(a.board().cells()==b.board().cells()&&bool(a.active())==bool(b.active()));
    if(a.active())CHECK(piece_equal(*a.active(),*b.active()));
    CHECK(a.soft_drop().remaining==b.soft_drop().remaining&&a.soft_drop().period==b.soft_drop().period);
    CHECK(a.gravity().elapsed==b.gravity().elapsed&&a.gravity().interval==b.gravity().interval);
    CHECK(a.score()==b.score()&&a.total_lines()==b.total_lines()&&a.level()==b.level());
    CHECK(a.last_awarded()==b.last_awarded()&&a.last_cleared()==b.last_cleared());
    CHECK(a.quarter()==b.quarter()&&a.last_rotation_candidate()==b.last_rotation_candidate());
    CHECK(a.kind()==b.kind()&&a.end_reason()==b.end_reason()&&a.source_cursor()==b.source_cursor());
    for(std::size_t i=0;i<3;++i)CHECK(a.next().peek(i)==b.next().peek(i));
}


static unsigned occupied(const study_grid::Grid& b){unsigned n=0;for(auto c:b.cells())n+=c==study_grid::Cell::filled;return n;}
int main() {
    using study_grid::Cell;using study_combat::insert;
    const int attacks[]={0,0,1,2,4};
    for(int n=-10;n<=10;++n){const auto a=study_combat::normal_attack(n);CHECK(bool(a)==(n>=0&&n<=4));if(a)CHECK(*a==attacks[n]);}
    CHECK(!study_combat::normal_attack(std::numeric_limits<int>::max()));
    for(int before=-1;before<=21;++before)for(int added:{-1,0,1,19,20,21,std::numeric_limits<int>::max()}) {
        const auto n=study_combat::add_pending(before,added);
        CHECK(bool(n)==(before>=0&&before<=20&&added>=0));
        if(n){const auto total=static_cast<long long>(before)+added;CHECK(*n==(total>20?20:total));}
    }
    // Every single marker against every height and hole. Independent destination
    // mapping from the marker's old coordinate, plus the nine cells per new row.
    unsigned cases=0;
    for(int marker=-1;marker<200;++marker)for(int rows=0;rows<=20;++rows)for(int hole=0;hole<10;++hole){
        study_grid::Grid board;if(marker>=0)CHECK(board.set(marker/10,marker%10,Cell::filled));
        const auto original=board.cells();const auto r=insert(board,rows,hole);CHECK(r);
        CHECK(board.cells()==original&&r->rows==rows);
        CHECK(r->overflow==(marker>=0&&marker/10<rows));
        for(int y=0;y<20;++y)for(int x=0;x<10;++x){
            const bool expected=(y>=20-rows&&x!=hole)||(marker>=0&&marker/10>=rows&&y==marker/10-rows&&x==marker%10);
            CHECK((r->board.get(y,x)==Cell::filled)==expected);
        }
        ++cases;
    }
    study_grid::Grid empty;
    for(int n:{-1,21,std::numeric_limits<int>::max()})CHECK(!insert(empty,n,4));
    for(int hole:{-1,10,std::numeric_limits<int>::max()})CHECK(!insert(empty,0,hole));
    auto left=*Round::create(make_clear_board(Kind::O),Kind::O,30);
    auto right=*Round::create({},Kind::I,30);
    study_combat::Duel duel(left,right);
    auto failed=duel.tick({0,false,false,true},{2,false,false,true});CHECK(!failed);
    same(duel.left(),left);same(duel.right(),right);
    const auto first=duel.tick({0,false,false,true},{0,false,false,true});CHECK(first);
    CHECK(first->left_attack==1&&first->right_attack==0);
    CHECK(duel.right().pending_garbage()==1&&duel.right().last_garbage()==0&&duel.right().hole_cursor()==0);
    CHECK(duel.left().pending_garbage()==0&&occupied(duel.right().board())==4);
    const auto second=duel.tick({0,false,false,false},{0,false,false,true});CHECK(second);
    CHECK(second->left_attack==0&&second->right_attack==0);
    CHECK(duel.right().pending_garbage()==0&&duel.right().last_garbage()==1&&duel.right().hole_cursor()==1);
    CHECK(occupied(duel.right().board())==17); // two I pieces +9 new cells
    // Swapping boards/inputs swaps outcomes: no first-player delivery advantage.
    study_combat::Duel mirror(right,left);
    CHECK(mirror.tick({0,false,false,true},{0,false,false,true}));
    CHECK(mirror.tick({0,false,false,true},{0,false,false,false}));
    same(mirror.left(),duel.right());same(mirror.right(),duel.left());
    // Both clear two rows on the same tick: incoming attack waits until next lock.
    study_combat::Duel both(left,left);const auto together=both.tick({0,false,false,true},{0,false,false,true});CHECK(together);
    CHECK(together->left_attack==1&&together->right_attack==1);
    CHECK(both.left().pending_garbage()==1&&both.right().pending_garbage()==1);
    CHECK(both.left().last_garbage()==0&&both.right().last_garbage()==0);
    // Grouping deliveries changes neither the counter nor hole-source consumption.
    auto grouped=right,split=right;CHECK(grouped.add_garbage(3));CHECK(split.add_garbage(1)&&split.add_garbage(2));
    same(grouped,split);CHECK(grouped.tick(0,false,false,true)==Step::locked);CHECK(split.tick(0,false,false,true)==Step::locked);same(grouped,split);
    CHECK(grouped.hole_cursor()==1&&grouped.last_garbage()==3);
    auto cycling=right;
    const int holes[]={4,8,1,4};
    for(unsigned batch=0;batch<4;++batch) {
        CHECK(cycling.add_garbage(1));CHECK(cycling.tick(0,false,false,true)==Step::locked);
        CHECK(cycling.hole_cursor()==(batch+1)%3);
        for(int c=0;c<10;++c)CHECK((cycling.board().get(19,c)==Cell::empty)==(c==holes[batch]));
    }
    const auto query=grouped;(void)grouped.ghost();same(grouped,query);
    CHECK(!grouped.add_garbage(-1));same(grouped,query);
    CHECK(grouped.tick(2,false,false,true)==Step::invalid);same(grouped,query);
    // Empty rows leaving the top are legal; an occupied edge cell is overflow.
    auto board=empty;CHECK(board.set(0,0,Cell::filled));
    auto lost=*Round::create(board,Kind::I);CHECK(lost.add_garbage(1));
    CHECK(lost.tick(0,false,false,true)==Step::game_over);
    CHECK(lost.end_reason()==EndReason::garbage_overflow&&!lost.active()&&!lost.ghost());
    CHECK(lost.last_garbage()==1&&lost.hole_cursor()==1&&lost.next().size()==3);
    const auto end=lost;CHECK(!lost.add_garbage(1));CHECK(lost.tick(0,false,false,true)==Step::stopped);same(lost,end);
    // Clear first, then garbage: cells in completed top rows do not overflow.
    auto rescue=*Round::create(*spawn_example::make(Kind::O,spawn_example::Scenario::clear_rescue),Kind::O);
    CHECK(rescue.add_garbage(1));CHECK(rescue.tick(0,false,false,true)==Step::game_over);
    CHECK(rescue.end_reason()==EndReason::spawn_blocked&&rescue.last_cleared()==2&&rescue.last_garbage()==1);
    CHECK(rescue.attack_sent()==1); // final clear still awards before a blocked spawn
    // A huge delivery is clamped before signed addition and before any row loop.
    auto huge=right;CHECK(huge.add_garbage(std::numeric_limits<int>::max()));CHECK(huge.add_garbage(1));
    CHECK(huge.pending_garbage()==20&&huge.hole_cursor()==0);
    CHECK(huge.tick(0,false,false,true)==Step::game_over&&huge.last_garbage()==20);
    // The dead side ignores incoming delivery, but its generated attack can be sent.
    study_combat::Duel terminal(end,left);CHECK(terminal.tick({0,false,false,false},{0,false,false,true}));
    same(terminal.left(),end);CHECK(terminal.right().attack_sent()==1);
    std::printf("%u insertion maps, capped counters, independent delivery order, rollback, grouping, overflow/spawn boundaries passed\n",cases);
}
