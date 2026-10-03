#include "net/callback_loop.h"
#include "net/probe_frame.h"
#include "net/receive_socket.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
using namespace study_net;
using namespace study_fixture;
int main() {
    Runtime runtime;if(!runtime.ready())return 2;
    try {
        CallbackLoop loop(make_poll_reactor());
        auto sockets=pair();
        FrameParser parser;bool received=false;
        const auto id=loop.attach(std::move(sockets.receiver),Read,
            [&](CallbackLoop& owner,Socket& socket,const ReadyEvent& event){
                std::uint8_t bytes[16];
                const auto read=try_receive(socket,bytes,sizeof(bytes));
                if(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted)return;
                require(read.state==ReceiveState::progress&&parser.append(bytes,read.count),"receive frame");
                Frame frame;const auto parsed=parser.next(frame);
                if(parsed==ParseStatus::need_more)return;
                require(parsed==ParseStatus::frame&&matches_probe(frame,12,7),"frame identity");
                received=true;
                require(owner.close(event.id),"self unregister");
                // The executing handler and Socket remain alive until this callback returns.
                std::puts("CALLBACK: complete frame; registration removed; execution still owns its state");
            });
        require(bool(id),"attach");
        EncodedFrame frame;require(encode_frame(probe_frame(12,7),frame),"encode");
        const auto sent=send_bounded(sockets.sender,frame.bytes.data(),frame.size,
            std::chrono::steady_clock::now()+std::chrono::seconds(2));
        require(sent.outcome==SendOutcome::complete,"send frame");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!received&&std::chrono::steady_clock::now()<deadline){
            const auto batch=loop.observe(50);require(batch.state!=PollState::error,"observe");
            loop.dispatch(batch);
        }
        require(received&&!loop.current(*id),"completed/removed");
        require(loop.wake(),"wake");
        const auto notification=loop.observe(1000);
        require(notification.woken&&notification.events.empty(),"wake has no connection payload");
        std::puts("LOOP: stale registration rejected; wake returned without a connection callback");
    } catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
