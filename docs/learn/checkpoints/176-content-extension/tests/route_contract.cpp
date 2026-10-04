#include "bot/route_player.h"
#include <limits>
#include <iostream>
#include <stdexcept>

void require(bool ok){if(!ok)throw std::runtime_error("route contract");}
int main() {
    using study_route::Status;
    std::size_t routes=0,inputs=0;
    for(unsigned seed=1;seed<=10;++seed)for(int interval:{1,7,30}) {
        auto live=*study_round::Round::create_seeded({},seed,interval);
        for(int turn=0;turn<12 && !live.finished();++turn) {
            const auto actions=study_actions::legal_actions(live);
            if(actions.empty())break;
            for(int action:actions) {
                auto replay=live;const auto planned=study_actions::plan(live,action);
                study_route::Player player;
                require(player.status()==Status::idle && !player.issue(live));
                const auto before=study_hash::state_bytes(replay);
                require(player.start(replay,action) && study_route::same(before,replay));
                while(player.status()!=Status::complete) {
                    const auto mask=player.issue(replay);require(mask.has_value());
                    require(player.status()==Status::awaiting_feedback && !player.issue(replay));
                    const auto input=*study_input::decode(*mask);
                    replay.tick(input.horizontal,input.clockwise,input.soft_drop,input.hard_drop);
                    require(player.observe(replay));require(!player.observe(replay));
                    ++inputs;
                }
                require(player.confirmed()==planned->inputs.size());
                require(study_route::same(study_hash::state_bytes(planned->result),replay));
                require(!player.issue(replay));++routes;
            }
            live=study_actions::plan(live,actions[(seed+turn)%actions.size()])->result;
        }
    }
    require(routes>0 && inputs>=routes);
    std::cout<<"routes="<<routes<<" confirmed inputs="<<inputs<<'\n';
    auto live=*study_round::Round::create({},study_catalog::Kind::O);
    const int target=*study_actions::encode(live.active()->origin.column,0);
    study_route::Player p;
    require(p.start(live,target));
    live.tick(0); // Even unchanged visible geometry has a different gravity phase.
    require(!p.issue(live) && p.status()==Status::stale);
    require(p.start(live,target));require(p.issue(live).has_value());
    require(!p.observe(live) && p.status()==Status::stale); // Command not applied.
    require(p.start(live,target));require(p.issue(live).has_value());
    live.tick(0);require(!p.observe(live) && p.status()==Status::stale); // Wrong tick.
    require(!p.start(live,std::numeric_limits<int>::max()) && p.status()==Status::rejected);
    require(!p.issue(live));p.cancel();require(p.status()==Status::idle && p.confirmed()==0);
    study_grid::Grid wall;
    for(int row=0;row<study_grid::Grid::kRows;++row)require(wall.set(row,2,study_grid::Cell::filled));
    auto blocked=*study_round::Round::create(wall,study_catalog::Kind::O);
    const auto before=study_hash::state_bytes(blocked);
    require(!p.start(blocked,*study_actions::encode(0,0)) && study_route::same(before,blocked));
}
