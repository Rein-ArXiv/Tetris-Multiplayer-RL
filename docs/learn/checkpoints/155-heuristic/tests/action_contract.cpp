#include "simulation/action_plan.h"
#include "simulation/state_hash.h"
#include <limits>
#include <stdexcept>
#include <cstring>

void require(bool value) {if(!value)throw std::runtime_error("action contract");}
bool same(const study_round::Round& a,const study_round::Round& b) {
    const auto x=study_hash::state_bytes(a),y=study_hash::state_bytes(b);
    return x.ok()&&y.ok()&&x.size()==y.size()&&std::memcmp(x.data(),y.data(),x.size())==0;
}
int main() {
    using namespace study_actions;
    for(int action=0;action<kCount;++action) {
        const auto target=decode(action);
        require(target && encode(target->column,target->quarter)==action);
    }
    require(!decode(-1)&&!decode(kCount)&&!decode(std::numeric_limits<int>::max()));
    require(!encode(-1,0)&&!encode(0,kOrientations)&&!encode(std::numeric_limits<int>::max(),0));
    for(std::uint64_t seed=1;seed<=16;++seed)for(int interval:{1,7,30}) {
        auto current=*study_round::Round::create_seeded({},seed,interval);
        for(int turn=0;turn<30 && !current.finished();++turn) {
            const auto before=current;
            const auto actions=legal_actions(current);
            require(same(before,current));
            if(actions.empty())break;
            for(const auto action:actions) {
                const auto selected=plan(current,action);
                require(selected && !selected->inputs.empty());
                auto replay=current;
                for(std::size_t i=0;i<selected->inputs.size();++i) {
                    const auto input=*study_input::decode(selected->inputs[i]);
                    const auto step=replay.tick(input.horizontal,input.clockwise,input.soft_drop,input.hard_drop);
                    if(i+1<selected->inputs.size())require(step==study_round::Step::waiting || step==study_round::Step::changed);
                }
                require(same(replay,selected->result));
            }
            current=plan(current,actions[(seed+turn)%actions.size()])->result;
        }
    }
    // A clear endpoint across a solid wall is not reachable by this input route.
    study_grid::Grid wall;
    for(int row=0;row<study_grid::Grid::kRows;++row)
        require(wall.set(row,2,study_grid::Cell::filled));
    auto blocked=*study_round::Round::create(wall,study_catalog::Kind::O);
    auto target=*blocked.active();target.origin.column=0;
    require(study_ghost::project(wall,target).has_value());
    require(!plan(blocked,*encode(0,0)));
    // At the floor, any preparatory tick locks; only the immediate drop survives.
    auto floor=*study_round::Round::create({},study_catalog::Kind::O,1);
    while(floor.ghost()->distance>0)floor.tick(0);
    require(!plan(floor,*encode(floor.active()->origin.column,1)));
    require(plan(floor,*encode(floor.active()->origin.column,0)).has_value());
    while(!floor.finished())floor.tick(0,false,false,true);
    require(legal_actions(floor).empty());
}
