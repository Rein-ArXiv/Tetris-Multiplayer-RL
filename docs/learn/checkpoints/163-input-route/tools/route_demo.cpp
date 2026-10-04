#include "bot/route_player.h"
#include <iostream>
int main() {
    auto live=*study_round::Round::create_seeded({},91);
    for(int turn=0;turn<5 && !live.finished();++turn) {
        const auto legal=study_actions::legal_actions(live);
        if(legal.empty())break;
        const int action=legal.front();
        study_route::Player player;
        if(!player.start(live,action))return 1;
        while(player.status()!=study_route::Status::complete) {
            const auto mask=player.issue(live);
            if(!mask)return 2;
            const auto input=*study_input::decode(*mask);
            const auto step=live.tick(input.horizontal,input.clockwise,input.soft_drop,input.hard_drop);
            if(!player.observe(live))return 3;
            std::cout<<"target="<<action<<" confirmed="<<player.confirmed()
                     <<" input="<<unsigned(*mask)<<" step="<<static_cast<int>(step)<<'\n';
        }
    }
}
