#include "simulation/round.h"
#include "src/spawn_example.h"
#include "src/end_message.h"
#include <cstdio>
int main(){
    const auto source=*study_next::ScriptedSource::cycle(study_catalog::Kind::O);
    for(auto scenario:{spawn_example::Scenario::initial_blocked,spawn_example::Scenario::next_blocked,spawn_example::Scenario::clear_rescue}){
        auto round=*study_round::Round::create(*spawn_example::make(study_catalog::Kind::O,scenario),source,1);
        (void)round.tick(0);
        std::printf("%s | cleared=%d cursor=%zu next=%zu\n",study_ui::end_message(round.end_reason()),round.last_cleared(),round.source_cursor(),round.next().size());
    }
}
