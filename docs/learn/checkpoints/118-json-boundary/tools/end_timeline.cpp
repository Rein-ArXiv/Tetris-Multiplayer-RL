#include "net/end_negotiation.h"
#include "net/round_play.h"
#include <cstdio>

int main(){
    using namespace study_net;
    const auto board=study_round::Round::create_seeded(study_grid::Grid{},77);
    if(!board)return 1;
    const study_combat::Duel duel(*board,*board);
    RoundPlay host,peer;
    auto a=EndNegotiation::create(1,30000),b=EndNegotiation::create(1,30000);
    if(!a||!b||!host.prepare(1,duel,Side::host,0)||!peer.prepare(1,duel,Side::peer,0)||
       !host.start()||!peer.start())return 1;
    Frame hf,pf,choice;
    for(unsigned tick=0;tick<1000;++tick){
        if(host.capture(study_input::drop,hf).input!=Put::stored||peer.capture(study_input::drop,pf).input!=Put::stored||
           host.receive(pf).input!=Put::stored||peer.receive(hf).input!=Put::stored||host.advance()!=Advance::advanced)return 1;
        if(host.gate().phase()!=RoundPhase::ended){if(peer.advance()!=Advance::advanced)return 1;continue;}
        std::printf("local terminal: %llu completed ticks\n",static_cast<unsigned long long>(host.game()->next_tick()));
        if(!a->activate(100)||a->choose(EndChoice::restart,101)!=ChoicePut::stored||
           !encode_end_choice({1,EndChoice::restart},choice))return 1;
        EndChoiceMessage message;
        if(!decode_end_choice(choice,message)||b->receive(message,90)!=ChoicePut::stored||b->status(90)!=EndState::dormant)return 1;
        std::puts("early peer intention staged; local game is still running");
        if(peer.advance()!=Advance::advanced||peer.gate().phase()!=RoundPhase::ended||!b->activate(100)||
           b->choose(EndChoice::restart,101)!=ChoicePut::stored||!encode_end_choice({1,EndChoice::restart},choice)||
           !decode_end_choice(choice,message)||a->receive(message,102)!=ChoicePut::stored)return 1;
        if(a->status(102)!=EndState::restart_agreed||b->status(102)!=EndState::restart_agreed)return 1;
        // The fixture supplies the agreed new round configuration to both owners.
        if(!host.prepare(2,duel,Side::host,0)||!peer.prepare(2,duel,Side::peer,0))return 1;
        std::puts("both restart intentions; prepare round 2 with an explicitly supplied configuration");
        auto disconnected=EndNegotiation::create(2,30000);
        if(!disconnected||disconnected->transport_ended(0)!=EndState::transport_lost)return 1;
        std::puts("EOF: transport lost, no winner or reward inferred");
        return 0;
    }
    return 1;
}
