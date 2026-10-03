#include "net/thread_link.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include "net/stream.h"
#include "net/hash_protocol.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
using namespace study_net;
using Clock = std::chrono::steady_clock;
Frame frame(unsigned n) { Frame f; CHECK(encode_stamp({n*4u,n,~std::uint64_t(n)},f)); return f; }
void check_frame(const Frame& f,unsigned n) { StateStamp s;CHECK(decode_stamp(f,s));CHECK(s.tick==n*4u && s.host_hash==n && s.peer_hash==~std::uint64_t(n)); }
std::pair<Socket,Socket> sockets() {
    int e=0;std::uint16_t port=0;auto listener=listen_loopback(0,port,e);CHECK(listener.valid());
    auto a=connect_loopback(port,e);CHECK(a.valid());auto b=accept_one(listener,e);CHECK(b.valid());
    return {std::move(a),std::move(b)};
}
LinkReport wait_report(ThreadLink& link) {
    const auto limit=Clock::now()+std::chrono::seconds(5);
    while(Clock::now()<limit) { if(auto r=link.report())return *r;std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    CHECK(false); return {LinkEnd::cancelled};
}
void send_bytes(Socket& s,const std::uint8_t* p,std::size_t n) {
    int e=0;CHECK(set_nonblocking(s,true,e));
    CHECK(send_bounded(s,p,n,Clock::now()+std::chrono::seconds(2)).outcome==SendOutcome::complete);
}
void queues() {
    FrameQueue<2> q;Frame f=frame(99);CHECK(q.try_pop(f)==QueueGet::empty);check_frame(f,99);
    CHECK(q.try_push(frame(0))==QueuePut::stored);CHECK(q.try_push(frame(1))==QueuePut::stored);
    CHECK(q.try_push(frame(2))==QueuePut::full);CHECK(q.try_pop(f)==QueueGet::item);check_frame(f,0);
    CHECK(q.try_push(frame(2))==QueuePut::stored);q.close();q.close();
    CHECK(q.try_push(frame(3))==QueuePut::closed);
    CHECK(q.try_pop(f)==QueueGet::item);check_frame(f,1);CHECK(q.try_pop(f)==QueueGet::item);check_frame(f,2);
    CHECK(q.try_pop(f)==QueueGet::closed);check_frame(f,2);
    Frame bad=frame(4);bad.size=33;CHECK(q.try_push(bad)==QueuePut::invalid);
    FrameQueue<1> copy;auto original=frame(5);CHECK(copy.try_push(original)==QueuePut::stored);original=frame(6);
    CHECK(copy.try_pop(f)==QueueGet::item);check_frame(f,5);
    FrameQueue<7> shared;
    std::thread producer([&] {for(unsigned i=0;i<10000;++i) {
        QueuePut result;while((result=shared.try_push(frame(i)))==QueuePut::full)std::this_thread::yield();
        CHECK(result==QueuePut::stored);
    }shared.close();});
    unsigned count=0;for(;;){auto result=shared.try_pop(f);if(result==QueueGet::closed)break;
        if(result==QueueGet::empty){std::this_thread::yield();continue;}check_frame(f,count++);}
    producer.join();CHECK(count==10000);
}
void receive_attempts() {
    Socket invalid;std::uint8_t data[16];CHECK(try_receive(invalid,data,16).state==ReceiveState::error);
    auto pair=sockets();int e=0;CHECK(set_nonblocking(pair.first,true,e));
    CHECK(try_receive(pair.first,nullptr,1).state==ReceiveState::error);
    CHECK(try_receive(pair.first,data,0).state==ReceiveState::error);
    CHECK(try_receive(pair.first,data,16).state==ReceiveState::would_block);
    CHECK(shutdown_send(pair.second,e));
    auto limit=Clock::now()+std::chrono::seconds(2);ReceiveAttempt result{};
    do {result=try_receive(pair.first,data,16);if(result.state==ReceiveState::eof)break;std::this_thread::yield();} while(Clock::now()<limit);
    CHECK(result.state==ReceiveState::eof && result.count==0);
}
void workers() {
    { auto pair=sockets();ThreadLink a(std::move(pair.first)),b(std::move(pair.second));
      for(unsigned i=0;i<4;++i){CHECK(a.send(frame(i))==QueuePut::stored);CHECK(b.send(frame(i))==QueuePut::stored);}
      a.finish_sending();b.finish_sending();CHECK(a.send(frame(4))==QueuePut::closed);
      CHECK(wait_report(a).end==LinkEnd::complete);CHECK(wait_report(b).end==LinkEnd::complete);
      Frame f;for(unsigned i=0;i<4;++i){CHECK(a.receive(f)==QueueGet::item);check_frame(f,i);CHECK(b.receive(f)==QueueGet::item);check_frame(f,i);}
      CHECK(a.receive(f)==QueueGet::closed);CHECK(b.receive(f)==QueueGet::closed);
    }
    {auto pair=sockets();ThreadLink a(std::move(pair.first));a.request_stop();CHECK(wait_report(a).end==LinkEnd::cancelled);}
    // Destruction must cancel a silent connected peer rather than joining a blocking recv.
    for(int i=0;i<20;++i){auto pair=sockets();ThreadLink a(std::move(pair.first));}
    {ThreadLink invalid(Socket{});CHECK(wait_report(invalid).end==LinkEnd::io_error);}
    for(int kind=0;kind<3;++kind){auto pair=sockets();ThreadLink a(std::move(pair.first));int e=0;
      if(kind==0){const std::uint8_t data[]={0,0};send_bytes(pair.second,data,2);}
      if(kind==1){const std::uint8_t data[]={1};send_bytes(pair.second,data,1);CHECK(shutdown_send(pair.second,e));}
      if(kind==2){for(unsigned i=0;i<9;++i){EncodedFrame data;CHECK(encode_frame(frame(i),data));send_bytes(pair.second,data.bytes.data(),data.size);}}
      CHECK(wait_report(a).end==(kind==0?LinkEnd::protocol_error:kind==1?LinkEnd::truncated:LinkEnd::receive_full));
      if(kind==2){Frame f;for(unsigned i=0;i<8;++i){CHECK(a.receive(f)==QueueGet::item);check_frame(f,i);}CHECK(a.receive(f)==QueueGet::closed);}
    }
    // Peer EOF alone is not whole-connection completion; local outbound may still send.
    {auto pair=sockets();ThreadLink a(std::move(pair.first));int e=0;CHECK(shutdown_send(pair.second,e));
     CHECK(a.send(frame(3))==QueuePut::stored);a.finish_sending();CHECK(wait_report(a).end==LinkEnd::complete);
     FrameParser parser;Frame f;std::uint8_t data[16];bool seen=false;
     for(;;){auto got=receive_some(pair.second,data,sizeof(data));if(got.status==StreamStatus::eof)break;
       CHECK(got.status==StreamStatus::progress);CHECK(parser.append(data,got.count));
       if(parser.next(f)==ParseStatus::frame){CHECK(!seen);check_frame(f,3);seen=true;}}
     CHECK(seen && parser.pending_bytes()==0);
    }
}
int main(){Runtime runtime;CHECK(runtime.ready());queues();receive_attempts();workers();std::puts("thread contracts: FIFO/10000 handoffs/copy/close/idle cancel/framing/overflow/half-close passed");}
