#include "src/sim_game.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"current soft drop line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main(){
    // Only SubmitInput calls define this cooldown's time; no gravity in this test.
    for(unsigned seed=1;seed<=7;++seed)for(unsigned mask=0;mask<4096;++mask){
        SimGame game(seed);int expected=game.CurrentRow(),next=1;
        for(int call=1;call<=12;++call){
            const bool held=(mask&(1u<<(call-1)))!=0;
            if(!held)next=call+1;else if(call>=next){++expected;next=call+4;}
            game.SubmitInput(held?INPUT_DOWN:0);CHECK(game.CurrentRow()==expected);
        }
    }
    // A due gravity step and soft step are additive on a clear path.
    SimGame both(1);for(int n=0;n<29;++n){both.SubmitInput(0);both.Tick();}
    CHECK(both.CurrentRow()==0);both.SubmitInput(INPUT_DOWN);CHECK(both.CurrentRow()==1);
    both.Tick();CHECK(both.CurrentRow()==2);
    // Rendering/queries do not spend the cooldown; neither do Tick-only calls.
    SimGame query(1);query.SubmitInput(INPUT_DOWN);auto hash=query.StateHash();
    for(int n=0;n<100;++n){(void)query.GhostBlock();CHECK(query.StateHash()==hash);}
    for(int n=0;n<10;++n)query.Tick();
    query.SubmitInput(INPUT_DOWN);CHECK(query.CurrentRow()==1); // still cooling
    query.SubmitInput(0);query.SubmitInput(INPUT_DOWN);CHECK(query.CurrentRow()==2);
    // Same visible pose/board, different repeat phase, then different next DOWN result.
    SimGame a(1),b(1);a.SubmitInput(INPUT_DOWN);b.MoveBlockDown();
    CHECK(a.CurrentRow()==b.CurrentRow()&&a.CurrentCol()==b.CurrentCol());
    CHECK(a.StateHashBreakdown().grid==b.StateHashBreakdown().grid);
    CHECK(a.StateHashBreakdown().currentBlock==b.StateHashBreakdown().currentBlock);
    CHECK(a.StateHash()!=b.StateHash());a.SubmitInput(INPUT_DOWN);b.SubmitInput(INPUT_DOWN);
    CHECK(a.CurrentRow()+1==b.CurrentRow());
    // Observe release in a simulation call; OS release that was never sampled is not a reset.
    SimGame held(1);held.SubmitInput(INPUT_DOWN);
    held.SubmitInput(INPUT_DOWN);held.SubmitInput(INPUT_DOWN);held.SubmitInput(INPUT_DOWN);
    CHECK(held.CurrentRow()==1);held.SubmitInput(INPUT_DOWN);CHECK(held.CurrentRow()==2);
    held.gameOver=true;hash=held.StateHash();held.SubmitInput(0);held.Tick();CHECK(held.StateHash()==hash);
    std::puts("current soft drop: 28672 held histories, 4-call period, observed release, additive gravity, query preservation and hidden phase hash passed");
}
