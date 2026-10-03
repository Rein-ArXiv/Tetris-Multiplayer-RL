#include "net/match_submission.h"
#include "net/round_play.h"
#include "net/socket.h"
#include "net/stream.h"
#include "net/send_socket.h"
#include "net/receive_socket.h"
#include "simulation/state_hash.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace study_net;
void require(bool ok) { if (!ok) throw std::runtime_error("match submission probe"); }
struct Peer { Socket socket; FrameParser parser; };
void send_frame(Socket& socket,const Frame& frame) {
    EncodedFrame encoded;require(encode_frame(frame,encoded));
    std::size_t sent=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(sent<encoded.size) {
        require(std::chrono::steady_clock::now()<deadline);
        const auto r=try_send(socket,encoded.bytes.data()+sent,encoded.size-sent);
        if(r.state==SendState::progress) {require(r.count>0);sent+=r.count;}
        else {
            require(r.state==SendState::would_block || r.state==SendState::interrupted);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
Frame read_frame(Peer& peer) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    for (;;) {
        Frame frame;const auto p=peer.parser.next(frame);
        if(p==ParseStatus::frame)return frame;
        require(p==ParseStatus::need_more && std::chrono::steady_clock::now()<deadline);
        std::uint8_t bytes[3];const auto r=try_receive(peer.socket,bytes,sizeof(bytes));
        if(r.state==ReceiveState::progress)require(peer.parser.append(bytes,r.count));
        else {
            require(r.state==ReceiveState::would_block || r.state==ReceiveState::interrupted);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
bool same(const study_round::Round& a,const study_round::Round& b) {
    const auto x=study_hash::state_bytes(a),y=study_hash::state_bytes(b);
    return x.ok() && y.ok() && x.size()==y.size() && std::equal(x.data(),x.data()+x.size(),y.data());
}
// One-row in-memory test double. Destruction loses its data; this is not a DB.
struct ReceiptService {
    std::optional<MatchRecord> stored;
    unsigned writes=0;
    Reply operator()(const MatchRecord& record) noexcept {
        if(!stored) {
            stored=record;++writes;
            return {}; // Applied, but the caller learns nothing from this reply.
        }
        if(*stored!=record)return {}; // Never confirm a changed operation.
        return {Reply::confirmed,{record.key,1,record.player_a,record.player_b}};
    }
};
int main() {
    try {
        Runtime runtime;require(runtime.ready());
        int error=0;std::uint16_t port=0;
        auto listener=listen_loopback(0,port,error);require(listener.valid());
        std::array<Peer,2> clients,server;
        for(unsigned side=0;side<2;++side){
            clients[side].socket=connect_loopback(port,error);require(clients[side].socket.valid());
            server[side].socket=accept_one(listener,error);require(server[side].socket.valid());
        }
        listener.reset();
        // Use bounded nonblocking send/receive loops for the tiny fixture frames.
        for(auto& peer:clients)require(set_nonblocking(peer.socket,true,error));
        for(auto& peer:server)require(set_nonblocking(peer.socket,true,error));
        auto board=study_round::Round::create_seeded(study_grid::Grid{},77);require(bool(board));
        study_combat::Duel duel(*board,*board);
        RoundPlay left,right,authority;
        require(left.prepare(1,duel,Side::host,0) && right.prepare(1,duel,Side::peer,0) &&
                authority.prepare(1,duel,Side::host,0));
        require(left.start() && right.start() && authority.start());
        std::uint64_t ticks=0;
        while(authority.gate().phase()==RoundPhase::playing) {
            require(ticks<1000);
            Frame outgoing_a,outgoing_b;
            require(left.capture(study_input::drop,outgoing_a).input==Put::stored);
            require(right.capture(0,outgoing_b).input==Put::stored);
            send_frame(clients[0].socket,outgoing_a);send_frame(clients[1].socket,outgoing_b);
            const auto received_a=read_frame(server[0]),received_b=read_frame(server[1]);
            RoundBatch a,b;
            require(decode_round_input(received_a,a) && decode_round_input(received_b,b));
            require(a.round==1 && b.round==1 && a.inputs.first_tick==ticks && b.inputs.first_tick==ticks &&
                    a.inputs.count==1 && b.inputs.count==1);
            Frame ignored;
            require(authority.capture(a.inputs.masks[0],ignored).input==Put::stored);
            require(authority.receive(received_b).input==Put::stored);
            send_frame(server[1].socket,received_a);send_frame(server[0].socket,received_b);
            require(left.receive(read_frame(clients[0])).input==Put::stored);
            require(right.receive(read_frame(clients[1])).input==Put::stored);
            require(left.advance()==Advance::advanced && right.advance()==Advance::advanced &&
                    authority.advance()==Advance::advanced);
            ++ticks;
            const auto& trusted=authority.game()->state();
            for(const auto* client:{&left,&right}) {
                require(same(trusted.left(),client->game()->state().left()));
                require(same(trusted.right(),client->game()->state().right()));
            }
        }
        const auto& terminal=authority.game()->state();
        const bool a_lost=terminal.left().finished(),b_lost=terminal.right().finished();
        require(a_lost || b_lost);
        require(terminal.left().total_lines()<=std::numeric_limits<std::uint32_t>::max() &&
                terminal.right().total_lines()<=std::numeric_limits<std::uint32_t>::max());
        MatchRecord record;
        record.key=17;record.round=1;record.player_a=101;record.player_b=202;
        record.ticks=ticks;record.score_a=terminal.left().score();record.score_b=terminal.right().score();
        record.lines_a=static_cast<std::uint32_t>(terminal.left().total_lines());
        record.lines_b=static_cast<std::uint32_t>(terminal.right().total_lines());
        record.winner=a_lost && b_lost ? MatchRecord::draw : a_lost ? MatchRecord::b : MatchRecord::a;
        MatchSubmission submission(3);require(submission.prepare(record));
        ReceiptService service;
        require(submission.submit(service)==MatchSubmission::SubmitStep::unconfirmed);
        require(service.writes==1 && !submission.receipt());
        require(submission.submit(service)==MatchSubmission::SubmitStep::confirmed);
        require(service.writes==1 && submission.receipt()->row==1 && submission.attempts()==2);
        auto conflicting=record;++conflicting.score_a;
        require(service(conflicting).kind==Reply::unconfirmed && service.writes==1);
        require(submission.submit(service)==MatchSubmission::SubmitStep::no_work);
        std::printf("TCP inputs -> trusted terminal simulation (%llu ticks) -> two attempts, one test-service write, receipt 1\n",
                    static_cast<unsigned long long>(ticks));
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
