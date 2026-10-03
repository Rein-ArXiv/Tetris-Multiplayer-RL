#include "net/stream.h"
#include "net/round_play.h"
#include "simulation/state_hash.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace study_net;
#define REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);} }while(false)

// The fixture controls the entire small exchange. These blocking helpers are
// not a general multi-client server: CTest supplies an external time limit.
bool write_exact(Socket& socket, const std::uint8_t* data, std::size_t size) {
    std::size_t done=0;
    while(done<size){
        const auto r=send_some(socket,data+done,size-done);
        if(r.status!=StreamStatus::progress||r.count==0||r.count>size-done)return false;
        done+=r.count;
    }
    return true;
}
bool read_exact(Socket& socket, std::uint8_t* data, std::size_t size, std::size_t cap) {
    if(cap==0)return false;
    std::size_t done=0;
    while(done<size){
        const auto r=receive_some(socket,data+done,std::min(cap,size-done));
        if(r.status!=StreamStatus::progress||r.count==0||r.count>size-done)return false;
        done+=r.count;
    }
    return true;
}
struct WireCount {std::uint64_t ingress=0,egress=0;};

// Endpoints own framing/round policy. The intermediate copies fixture-sized
// bytes only; it never reads TYPE, round, mask, or a game state.
Frame transfer(Socket& source, Socket& destination, Socket* relay_in,
               Socket* relay_out, const Frame& frame, std::size_t cap, WireCount& count) {
    EncodedFrame encoded;
    REQUIRE(encode_frame(frame,encoded));
    REQUIRE(write_exact(source,encoded.bytes.data(),encoded.size));
    if(relay_in){
        REQUIRE(relay_out);
        std::array<std::uint8_t,kMaxFrameBytes> bytes{};
        REQUIRE(read_exact(*relay_in,bytes.data(),encoded.size,cap));
        count.ingress+=encoded.size;
        REQUIRE(write_exact(*relay_out,bytes.data(),encoded.size));
        count.egress+=encoded.size;
    }
    std::array<std::uint8_t,kMaxFrameBytes> received{};
    REQUIRE(read_exact(destination,received.data(),encoded.size,cap));
    REQUIRE(std::equal(received.begin(),received.begin()+encoded.size,encoded.bytes.begin()));
    FrameParser parser;
    REQUIRE(parser.append(received.data(),encoded.size));
    Frame result;
    REQUIRE(parser.next(result)==ParseStatus::frame);
    REQUIRE(parser.pending_bytes()==0);
    return result;
}
using Snapshot=std::array<std::vector<std::uint8_t>,2>;
Snapshot snapshot(const RoundPlay& game){
    const auto left=study_hash::state_bytes(game.game()->state().left());
    const auto right=study_hash::state_bytes(game.game()->state().right());
    REQUIRE(left.ok()&&right.ok());
    return {std::vector<std::uint8_t>(left.data(),left.data()+left.size()),
            std::vector<std::uint8_t>(right.data(),right.data()+right.size())};
}
Snapshot exchange(bool relayed,std::size_t cap){
    int error=0;std::uint16_t port=0;
    auto listener=listen_loopback(0,port,error);REQUIRE(listener.valid());
    Socket a,b,ra,rb;
    if(relayed){
        a=connect_loopback(port,error);REQUIRE(a.valid());
        ra=accept_one(listener,error);REQUIRE(ra.valid());
        b=connect_loopback(port,error);REQUIRE(b.valid());
        rb=accept_one(listener,error);REQUIRE(rb.valid());
    }else{
        b=connect_loopback(port,error);REQUIRE(b.valid());
        a=accept_one(listener,error);REQUIRE(a.valid());
    }
    listener.reset();
    const auto board=study_round::Round::create_seeded(study_grid::Grid{},77);REQUIRE(board);
    const study_combat::Duel duel(*board,*board);
    RoundPlay host,peer;
    REQUIRE(host.prepare(1,duel,Side::host,0)&&peer.prepare(1,duel,Side::peer,0));
    REQUIRE(host.start()&&peer.start());
    WireCount count;
    for(unsigned tick=0;tick<3;++tick){
        Frame x,y;
        REQUIRE(host.capture(study_input::drop,x).input==Put::stored);
        REQUIRE(peer.capture(0,y).input==Put::stored);
        x=transfer(a,b,relayed?&ra:nullptr,relayed?&rb:nullptr,x,cap,count);
        y=transfer(b,a,relayed?&rb:nullptr,relayed?&ra:nullptr,y,cap,count);
        REQUIRE(peer.receive(x).input==Put::stored&&host.receive(y).input==Put::stored);
        REQUIRE(host.advance()==Advance::advanced&&peer.advance()==Advance::advanced);
        REQUIRE(snapshot(host)==snapshot(peer));
    }
    REQUIRE(count.ingress==(relayed?108u:0u)&&count.egress==count.ingress);
    REQUIRE(host.game()->next_tick()==3&&peer.game()->next_tick()==3);
    const auto final=snapshot(peer);
    // Forwarding preserves even a future-round message. Endpoint policy rejects it.
    RoundBatch future;future.round=2;future.inputs.count=1;future.inputs.masks[0]=0;
    Frame bad;REQUIRE(encode_round_input(future,bad));
    bad=transfer(a,b,relayed?&ra:nullptr,relayed?&rb:nullptr,bad,cap,count);
    REQUIRE(peer.receive(bad).scope==RoundScope::future_round&&snapshot(peer)==final);
    REQUIRE(count.ingress==(relayed?126u:0u)&&count.egress==count.ingress);
    std::printf("%s cap=%zu: TCP connections=%u relay rx=%llu tx=%llu; 3 ticks equal; future round rejected at endpoint\n",
                relayed?"relay":"direct",cap,relayed?2u:1u,
                static_cast<unsigned long long>(count.ingress),static_cast<unsigned long long>(count.egress));
    return final;
}
int main(int argc,char** argv){
    if(argc==2&&std::strcmp(argv[1],"--help")==0){std::puts("topology_probe: controlled loopback direct/relay byte paths; no WAN latency benchmark");return 0;}
    if(argc!=1)return 2;
    Runtime runtime;REQUIRE(runtime.ready());
    for(std::size_t cap:{1,2,7,16}){
        const auto direct=exchange(false,cap);
        const auto relayed=exchange(true,cap);
        REQUIRE(direct==relayed);
    }
    std::puts("same canonical state bytes on direct and relayed paths; forwarding is not an authority verdict");
}
