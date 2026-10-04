#include "net/iocp_receive.h"
#include "net/probe_frame.h"
#include "net/send_socket.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
using namespace study_net;
using namespace study_fixture;
CompletionResult collect(IocpReceiver& receiver) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(std::chrono::steady_clock::now()<deadline) {
        const auto packet=receiver.poll(50);require(packet.state!=PacketState::error,"completion port error");
        if(auto result=receiver.take())return *result;
    }
    throw std::runtime_error("probe deadline: receiver destructor still collects pending I/O");
}
int main() {
    Runtime runtime;if(!runtime.ready())return 2;
    try {
        auto sockets=pair();IocpReceiver receiver(std::move(sockets.receiver));
        EncodedFrame encoded;require(encode_frame(probe_frame(12,8),encoded),"encode");
        require(receiver.post(1,2).accepted(),"submit receive before data");
        require(!receiver.post(2,16).accepted(),"one pending request");
        require(receiver.wake(),"post wake");
        require(receiver.poll(1000).state==PacketState::woken&&!receiver.take(),"wake is not receive completion");
        auto sent=send_bounded(sockets.sender,encoded.bytes.data(),2,std::chrono::steady_clock::now()+std::chrono::seconds(2));
        require(sent.outcome==SendOutcome::complete,"send prefix");
        const auto first=collect(receiver);
        require(first.kind==CompletionKind::data&&first.count<=2&&first.request==1,"partial completion");
        FrameParser parser;require(parser.append(first.bytes.data(),first.count),"append prefix");
        Frame frame;require(parser.next(frame)==ParseStatus::need_more,"completion not complete frame");
        sent=send_bounded(sockets.sender,encoded.bytes.data()+2,encoded.size-2,std::chrono::steady_clock::now()+std::chrono::seconds(2));
        require(sent.outcome==SendOutcome::complete,"send body");
        std::uint64_t request=2;bool parsed=false;
        while(!parsed) {
            require(receiver.post(request++,16).accepted(),"submit remaining receive");
            const auto got=collect(receiver);require(got.kind==CompletionKind::data,"remaining bytes");
            require(parser.append(got.bytes.data(),got.count),"append completion");
            const auto status=parser.next(frame);require(status!=ParseStatus::error,"parse frame");
            parsed=status==ParseStatus::frame;
        }
        require(matches_probe(frame,12,8),"payload identity");
        require(receiver.post(request++,16).accepted(),"pending receive for cancel");
        const auto cancel=receiver.request_cancel();require(cancel.state!=CancelState::error,"request cancel");
        require(collect(receiver).kind==CompletionKind::cancelled,"collect cancelled operation");
        std::puts("IOCP: prefix completion, complete frame, wake distinction and cancellation collected");
    }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
