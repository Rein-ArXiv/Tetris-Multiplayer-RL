#include "net/connection.h"
#include "net/stream.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace study_net;
using Clock=std::chrono::steady_clock;
using SS=Connection::StartState;
using RS=Connection::ReadState;
void send_bytes(Socket& peer,const uint8_t* data,size_t size) {
    size_t offset=0;
    while(offset<size) {auto r=send_some(peer,data+offset,size-offset);CHECK(r.status==StreamStatus::progress);offset+=r.count;}
}
Socket open(Connection& c,const Socket& listener,uint16_t port) {
    CHECK(c.start(port).state==SS::started);int error=0;
    auto peer=accept_one(listener,error);CHECK(peer.valid());return peer;
}
int main() {
    Runtime runtime;CHECK(runtime.ready());
    Connection c;Frame output;output.type=99;output.size=1;output.payload[0]=88;
    CHECK(c.next_frame(output).state==RS::closed && output.type==99);
    CHECK(c.start(0).state==SS::io_error && !c.active());
    int error=1;CHECK(!c.finish_sending(error) && error==0);
    uint16_t port=0;auto listener=listen_loopback(0,port,error);CHECK(listener.valid());
    {
        auto peer=open(c,listener,port);CHECK(c.start(0).state==SS::busy && c.active());
        const uint8_t two[]{2,0,7,42,1,0,8};send_bytes(peer,two,sizeof(two));
        CHECK(shutdown_send(peer,error));
        CHECK(c.next_frame(output).state==RS::frame && output.type==7 && output.payload[0]==42);
        CHECK(c.next_frame(output).state==RS::frame && output.type==8 && output.size==0);
        CHECK(c.next_frame(output).state==RS::eof && c.peer_eof() && c.active());
        CHECK(c.next_frame(output).state==RS::eof && output.type==8);
        // Peer ended only its sending direction. A response is still possible.
        Frame reply;reply.type=9;reply.size=1;reply.payload[0]=55;
        auto sent=c.send_frame(reply,Clock::now()+std::chrono::seconds(1));CHECK(sent.outcome==SendOutcome::complete && sent.accepted==4);
        CHECK(c.finish_sending(error) && c.finish_sending(error));CHECK(c.send_closed() && c.active());
        CHECK(c.send_frame(reply,Clock::now()).outcome==SendOutcome::invalid_request);
        const uint8_t expected[]{2,0,9,55};size_t offset=0;uint8_t buf[16];
        for(;;){auto r=receive_some(peer,buf,sizeof(buf));if(r.status==StreamStatus::eof)break;CHECK(r.status==StreamStatus::progress && offset+r.count<=4);for(size_t i=0;i<r.count;++i)CHECK(buf[i]==expected[offset+i]);offset+=r.count;}
        CHECK(offset==4);CHECK(c.start(port).state==SS::busy);
        c.close();c.close();CHECK(!c.active() && !c.send_closed() && !c.peer_eof() && c.pending_bytes()==0);
    }
    for(int broken=0;broken<2;++broken) {
        auto peer=open(c,listener,port);uint8_t bad[]{0,0};
        send_bytes(peer,bad,broken==0?1:2);CHECK(shutdown_send(peer,error));
        output.type=99;CHECK(c.next_frame(output).state==(broken==0?RS::truncated:RS::protocol_error));
        CHECK(!c.active() && output.type==99 && c.pending_bytes()==0);
        // New stream starts with a fresh parser, not the previous partial header/error.
        auto next=open(c,listener,port);const uint8_t good[]{1,0,10};send_bytes(next,good,3);CHECK(shutdown_send(next,error));
        CHECK(c.next_frame(output).state==RS::frame && output.type==10);
        CHECK(c.next_frame(output).state==RS::eof);c.close();
    }
    {
        auto peer=open(c,listener,port);Frame bad;bad.size=kMaxPayloadBytes+1;
        CHECK(c.send_frame(bad,Clock::now()).outcome==SendOutcome::invalid_request && c.active());
        Frame valid;valid.type=1;
        auto r=c.send_frame(valid,Clock::now()-std::chrono::seconds(1));
        CHECK(r.outcome==SendOutcome::timed_out && r.accepted==0 && !c.active());
        uint8_t b;CHECK(receive_some(peer,&b,1).status==StreamStatus::eof);
    }
    // Synchronous closed object can restart after failure; no implicit retry.
    {auto peer=open(c,listener,port);c.close();uint8_t b;CHECK(receive_some(peer,&b,1).status==StreamStatus::eof);}
    std::puts("Connection: startup/busy, bidirectional half-close, EOF, truncation, parser reset and restart passed");
}
