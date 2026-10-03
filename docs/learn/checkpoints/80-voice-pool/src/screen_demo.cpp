#include "client/application.h"
#include "simulation/state_hash.h"
#include <cstdio>
int main(){
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},1);
    if(!round)return 1;
    study_app::Application app(*round);
    std::printf("menu owns game=%d\n",app.game()!=nullptr);
    const auto start=app.advance(.1,{},true,false,false);
    if(!start||!app.game())return 1;
    std::printf("start owns game=%d phase=%llu frame=%d\n",app.game()!=nullptr,
        static_cast<unsigned long long>(app.game()->phase()),start->frame.has_value());
    if(!app.advance(0,{},false,false,true))return 1;
    const auto tick=app.advance(.017,{false,false,false,false,true,false},true,false,false);
    if(!tick||!tick->frame)return 1;
    std::printf("playing ticks=%u locked=%d\n",tick->frame->ticks,tick->frame->observations[0].lock.has_value());
    if(!app.advance(.1,{},false,true,true))return 1;
    std::printf("back owns game=%d\n",app.game()!=nullptr);
    if(!app.advance(.1,{},true,false,false)||!app.game())return 1;
    const bool same=study_hash::state_hash(app.game()->round())==study_hash::state_hash(*round);
    std::printf("restart baseline restored=%d phase=%llu\n",same,
        static_cast<unsigned long long>(app.game()->phase()));
    return same?0:1;
}
