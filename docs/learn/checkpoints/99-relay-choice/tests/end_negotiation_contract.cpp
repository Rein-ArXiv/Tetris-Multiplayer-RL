#include "net/end_negotiation.h"
#include <cstdio>
#include <limits>
using namespace study_net;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
int main(){
    CHECK(!EndNegotiation::create(0,10)&&!EndNegotiation::create(1,0));
    Frame frame;
    CHECK(encode_end_choice({0x0807060504030201ULL,EndChoice::leave},frame));
    CHECK(frame.type==42&&frame.size==9);
    for(unsigned i=0;i<8;++i)CHECK(frame.payload[i]==i+1);
    CHECK(frame.payload[8]==2);
    for(unsigned type=0;type<256;++type)for(unsigned size=0;size<=40;++size){
        auto f=frame;f.type=type;f.size=size;EndChoiceMessage out{9,EndChoice::restart};
        const bool ok=decode_end_choice(f,out);CHECK(ok==(type==42&&size==9));
        CHECK(out.round==(ok?0x0807060504030201ULL:9));
        CHECK(out.choice==(ok?EndChoice::leave:EndChoice::restart));
    }
    for(unsigned raw=0;raw<256;++raw){
        auto f=frame;f.payload[8]=raw;EndChoiceMessage out{9,EndChoice::restart};
        const bool valid=raw==1||raw==2;CHECK(decode_end_choice(f,out)==valid);
        if(!valid)CHECK(out.round==9&&out.choice==EndChoice::restart);
        const auto before=f;CHECK(encode_end_choice({1,static_cast<EndChoice>(raw)},f)==valid);
        if(!valid)CHECK(f.type==before.type&&f.size==before.size&&f.payload==before.payload);
    }
    auto zero=frame;for(unsigned i=0;i<8;++i)zero.payload[i]=0;
    EndChoiceMessage out{9,EndChoice::leave};CHECK(!decode_end_choice(zero,out)&&out.round==9);
    auto saved=frame;CHECK(!encode_end_choice({0,EndChoice::restart},frame)&&frame.payload==saved.payload);
    const auto max=(std::numeric_limits<std::uint64_t>::max)();
    CHECK(encode_end_choice({max,EndChoice::restart},frame)&&decode_end_choice(frame,out)&&out.round==max);

    // Every choice pair and both orders, including an early peer intention.
    for(EndChoice local:{EndChoice::restart,EndChoice::leave})
    for(EndChoice remote:{EndChoice::restart,EndChoice::leave})for(bool early:{false,true}){
        auto n=*EndNegotiation::create(7,10);CHECK(n.round()==7);
        if(early){CHECK(n.receive({7,remote},0)==ChoicePut::stored);CHECK(n.status(100)==EndState::dormant);}
        CHECK(n.activate(100));
        const auto local_put=n.choose(local,101);
        CHECK(local_put==((early&&remote==EndChoice::leave)?ChoicePut::closed:ChoicePut::stored));
        if(!early)CHECK(n.receive({7,remote},102)==(local==EndChoice::leave?ChoicePut::closed:ChoicePut::stored));
        CHECK(n.status(102)==(local==EndChoice::restart&&remote==EndChoice::restart?EndState::restart_agreed:EndState::leave));
        CHECK(n.choose(EndChoice::restart,103)==ChoicePut::closed);
    }
    auto n=*EndNegotiation::create(7,10);
    CHECK(n.choose(EndChoice::restart,0)==ChoicePut::inactive);
    CHECK(n.receive({7,EndChoice::restart},1)==ChoicePut::stored);
    CHECK(n.receive({7,EndChoice::restart},2)==ChoicePut::duplicate);
    CHECK(n.receive({7,EndChoice::leave},3)==ChoicePut::conflict);
    CHECK(n.receive({6,EndChoice::leave},max)==ChoicePut::old_round);
    CHECK(n.receive({8,EndChoice::leave},max)==ChoicePut::future_round);
    CHECK(n.receive({0,EndChoice::leave},max)==ChoicePut::invalid);
    CHECK(n.choose(static_cast<EndChoice>(3),max)==ChoicePut::invalid);
    CHECK(n.activate(4)&&n.choose(EndChoice::restart,5)==ChoicePut::stored);
    CHECK(n.status(5)==EndState::restart_agreed);
    CHECK(n.transport_ended(6)==EndState::transport_lost);
    CHECK(!n.activate(7)&&n.receive({7,EndChoice::restart},7)==ChoicePut::closed);

    auto local=*EndNegotiation::create(1,10);CHECK(local.activate(100));
    CHECK(local.choose(EndChoice::restart,101)==ChoicePut::stored);
    CHECK(local.choose(EndChoice::restart,102)==ChoicePut::duplicate);
    CHECK(local.choose(EndChoice::leave,103)==ChoicePut::conflict);
    CHECK(local.status(102)==EndState::clock_error);
    CHECK(local.receive({1,EndChoice::restart},102)==ChoicePut::clock_error);
    CHECK(local.transport_ended(102)==EndState::clock_error);
    CHECK(local.status(103)==EndState::waiting);
    CHECK(!local.activate(109)); // does not extend the start time
    CHECK(local.receive({1,EndChoice::restart},110)==ChoicePut::closed);
    CHECK(local.status(110)==EndState::timed_out&&local.transport_ended(111)==EndState::timed_out);
    auto last=*EndNegotiation::create(1,10);CHECK(last.activate(100));
    CHECK(last.choose(EndChoice::restart,108)==ChoicePut::stored);
    CHECK(last.receive({1,EndChoice::restart},109)==ChoicePut::stored&&last.status(110)==EndState::restart_agreed);
    auto near=*EndNegotiation::create(1,10);CHECK(near.activate(max-10));
    CHECK(near.status(max-1)==EndState::waiting&&near.status(max)==EndState::timed_out);
    auto overflow=*EndNegotiation::create(1,10);CHECK(overflow.activate(max-2));
    CHECK(overflow.status(max)==EndState::waiting&&overflow.status(0)==EndState::clock_error);
    for(bool activated:{false,true}){auto lost=*EndNegotiation::create(1,10);if(activated)CHECK(lost.activate(0));CHECK(lost.transport_ended(1)==EndState::transport_lost);}
    auto leave=*EndNegotiation::create(1,10);CHECK(leave.activate(0));CHECK(leave.choose(EndChoice::leave,1)==ChoicePut::stored);CHECK(leave.transport_ended(2)==EndState::leave);
    std::puts("end contracts: exact codec, output preservation, early intention, immutable choices, scope, deadline, clock, EOF");
}
