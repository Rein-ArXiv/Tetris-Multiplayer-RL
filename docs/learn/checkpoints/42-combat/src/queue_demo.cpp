#include "simulation/round.h"
#include "src/clear_example.h"
#include <cstdio>
static char letter(study_catalog::Kind kind){return study_catalog::find(kind)->name[0];}
static void print(const study_round::Round& round){
    std::printf("current=%c next=",letter(round.kind()));
    for(unsigned i=0;i<3;++i)std::printf("%c%s",letter(*round.next().peek(i)),i==2?"":" ");
    std::printf(" cursor=%zu\n",round.source_cursor());
}
int main(){
    auto source=*study_next::ScriptedSource::cycle(study_catalog::Kind::O);
    auto round=*study_round::Round::create(make_clear_board(study_catalog::Kind::O),source,1);
    print(round);
    for(int lock=0;lock<2;++lock){
        bool seen=false;
        for(int tick=0;tick<50;++tick){
            auto step=round.tick(0);if(step==study_round::Step::locked){seen=true;break;}
            if(step!=study_round::Step::changed)return 1;
        }
        if(!seen)return 1;
        print(round);
    }
    std::printf("caller source cursor=%zu\n",source.cursor());
}
