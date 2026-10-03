#include "net/result_notice.h"
#include "net/final_output.h"
#include "net/reactor_choice.h"
#include "tests/reactor_fixture.h"
#include <algorithm>
#include <iostream>
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
int main(int argc,char** argv){try{
    Runtime runtime;require(runtime.ready(),"socket runtime");auto link=pair();
    CallbackLoop loop(choose_reactor(epoll_argument(argc,argv)));
    std::atomic<std::size_t> total{0};FinalOutput<128,96,32> output(total,128);
    Frame prefix;prefix.type=8;prefix.size=1;prefix.payload[0]=42;
    ChannelView server{};server.match.state=MatchState::finished;server.match.winner=Winner::host;
    server.saving=ResultHandoff::Stage::unconfirmed;
    Frame notice;require(encode_notice({7,notice_reason(server)},notice),"notice encode");
    require(output.enqueue(prefix)==BufferResult::stored && output.enqueue(notice)==BufferResult::stored,"FIFO admission");
    output.begin_close(std::chrono::steady_clock::now()+2s);
    const auto writer=loop.attach(std::move(link.sender),Write,[&](CallbackLoop& owner,Socket& socket,const ReadyEvent& event){
        if(!event.writable)return;
        output.writable(std::chrono::steady_clock::now(),[&](const std::uint8_t* data,std::size_t count)noexcept{
            return try_send(socket,data,std::min(count,std::size_t(2)));
        });
        if(!output.wants_write())owner.close(event.id);
    });
    require(bool(writer),"writer attach");
    FrameParser parser;NoticeView reader(7);unsigned frames=0;bool eof=false;
    const auto end=std::chrono::steady_clock::now()+3s;
    while(!eof && std::chrono::steady_clock::now()<end){
        loop.dispatch(loop.observe(1));
        output.expire(std::chrono::steady_clock::now());
        if(output.state()==DeliveryState::timed_out)loop.close(*writer);
        std::uint8_t bytes[3];const auto read=try_receive(link.receiver,bytes,sizeof(bytes));
        if(read.state==ReceiveState::would_block || read.state==ReceiveState::interrupted)continue;
        if(read.state==ReceiveState::eof){eof=true;break;}
        require(read.state==ReceiveState::progress && parser.append(bytes,read.count),"receive append");
        for(;;){Frame f;const auto parsed=parser.next(f);if(parsed==ParseStatus::need_more)break;
            require(parsed==ParseStatus::frame,"parse frame");
            require(frames==0 ? f.type==8 : reader.accept(f),"FIFO/notice identity");++frames;
        }
    }
    require(eof && frames==2 && reader.reason()==NoticeReason::unconfirmed,"notice before EOF");
    require(output.state()==DeliveryState::drained && total==0,"drain release");
    std::cout<<reader.text()<<"\nactual TCP: prefix then complete notice, partial writes, EOF after drain\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
