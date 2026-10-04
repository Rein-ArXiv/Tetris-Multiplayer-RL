#include "net/bound_input_stream.h"
#include "net/reactor_choice.h"
#include "net/timer_heap.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
#include <memory>
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
Frame input(std::uint64_t round, std::uint32_t first, std::uint8_t mask) {
    RoundBatch batch;batch.round=round;batch.inputs.first_tick=first;batch.inputs.count=4;
    for(std::size_t i=0;i<4;++i)batch.inputs.masks[i]=mask;
    Frame frame;require(encode_round_input(batch,frame),"input encode");return frame;
}
void offer(Socket& socket,const Frame& frame) {
    EncodedFrame encoded;require(encode_frame(frame,encoded),"encode frame");
    require(send_bounded(socket,encoded.bytes.data(),encoded.size,
        TimerHeap::Clock::now()+1s).outcome==SendOutcome::complete,"send request");
}
int main(int argc,char**argv) {
    Runtime runtime;if(!runtime.ready())return 2;
    try {
        InputAuthority authority(9,11,22);require(authority.start(),"start");
        CallbackLoop loop(choose_reactor(epoll_argument(argc,argv)));
        std::size_t handled=0,stored=0,duplicates=0,denied=0;
        // These identities model trusted admission results, not a wire login.
        auto bind=[&](std::uint64_t actor) {
            auto link=pair();auto stream=std::make_shared<BoundInputStream>(authority,actor);
            require(bool(loop.attach(std::move(link.receiver),Read,
                [&,stream](CallbackLoop& owner,Socket& socket,const ReadyEvent& event) {
                    if(!(event.readable||event.error))return;
                    std::uint8_t bytes[16];const auto read=try_receive(socket,bytes,sizeof(bytes));
                    if(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted)return;
                    if(read.state!=ReceiveState::progress){owner.close(event.id);return;}
                    BoundInputStream::Report report;
                    const bool intact=stream->feed(bytes,read.count,report);
                    handled+=report.handled;stored+=report.stored;duplicates+=report.duplicates;denied+=report.denied;
                    if(!intact)owner.close(event.id);
                })),"bind connection");
            return std::move(link.sender);
        };
        auto host=bind(11),peer=bind(22),outsider=bind(99),anonymous=bind(0);
        auto process_until=[&](std::size_t expected) {
            const auto deadline=TimerHeap::Clock::now()+2s;
            while(handled<expected&&TimerHeap::Clock::now()<deadline)loop.dispatch(loop.observe(10));
            require(handled==expected,"bounded server processing");
        };
        const auto valid=input(9,0,1);
        auto forged=valid;forged.type=42;offer(host,forged); // forged server result
        forged.type=255;offer(host,forged);
        offer(host,input(8,0,1));offer(host,input(10,0,1));
        auto malformed=valid;malformed.payload[14]=0x80;offer(host,malformed);
        offer(host,input(9,32,1));
        offer(host,valid);offer(host,valid);offer(host,input(9,0,2));
        process_until(9);
        require(stored==1&&duplicates==1&&denied==7&&authority.next_tick()==0,"host cannot fill the other side");
        offer(outsider,input(9,0,0));offer(anonymous,input(9,0,0));process_until(11);
        std::uint8_t a=90,b=91;
        require(!authority.take(a,b)&&a==90&&b==91,"untrusted actor cannot supply peer input");
        offer(peer,input(9,0,0));process_until(12);
        for(int tick=0;tick<4;++tick)require(authority.take(a,b)&&a==1&&b==0,"server binding selects side");
        require(!authority.take(a,b)&&authority.next_tick()==4,"one accepted effect per tick");
        require(stored==2&&duplicates==1&&denied==9,"all outcomes classified");
        authority.close();
        std::puts("BOUNDARIES: 9 denials, 2 stored batches, 1 identical retry, 4 paired ticks; no client result changes server state");
    }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
