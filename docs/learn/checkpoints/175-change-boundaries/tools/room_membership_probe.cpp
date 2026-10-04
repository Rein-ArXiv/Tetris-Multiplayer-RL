#include "net/room_membership.h"
#include "net/forward_direction.h"
#include "net/acceptance_lobby.h"
#include "net/room_directory.h"
#include "net/first_admission.h"
#include "net/round_play.h"
#include "net/send_socket.h"
#include "net/receive_socket.h"
#include "net/stream.h"
#include "simulation/state_hash.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace study_net;
using Clock = std::chrono::steady_clock;
void require(bool ok) { if (!ok) throw std::runtime_error("room directory probe contract"); }
struct Peer { Socket socket; FrameParser parser; };
void send_frame(Socket& socket, const Frame& frame) {
    EncodedFrame encoded; require(encode_frame(frame,encoded));
    std::size_t done = 0;
    while (done < encoded.size) {
        auto sent = send_some(socket,encoded.bytes.data()+done,encoded.size-done);
        require(sent.status == StreamStatus::progress && sent.count != 0);
        done += sent.count;
    }
}
void nonblocking(Socket& socket) {
    int error = 0; require(set_nonblocking(socket,true,error));
}
Frame read_frame(Peer& peer) {
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    for (;;) {
        Frame frame; const auto parsed = peer.parser.next(frame);
        if (parsed == ParseStatus::frame) return frame;
        require(parsed == ParseStatus::need_more && Clock::now() < deadline);
        std::uint8_t bytes[3]; auto received = try_receive(peer.socket,bytes,sizeof(bytes));
        if (received.state == ReceiveState::progress) require(peer.parser.append(bytes,received.count));
        else {
            require(received.state == ReceiveState::would_block || received.state == ReceiveState::interrupted);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
Peer admit(Socket socket, AdmissionRequest& request) {
    nonblocking(socket);
    auto admission = *FirstAdmission::create(0,1000);
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    AdmissionState state = AdmissionState::waiting;
    while (state == AdmissionState::waiting) {
        require(Clock::now() < deadline);
        std::uint8_t bytes[3]; auto received = try_receive(socket,bytes,sizeof(bytes));
        if (received.state == ReceiveState::progress) state = admission.feed(bytes,received.count,1);
        else {
            require(received.state == ReceiveState::would_block || received.state == ReceiveState::interrupted);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    require(state == AdmissionState::routed);
    AdmissionHandoff handoff; require(admission.take(handoff));
    request = handoff.request;
    return {std::move(socket),std::move(handoff.parser)};
}
using Snapshot = std::array<std::vector<std::uint8_t>,2>;
Snapshot snapshot(const RoundPlay& game) {
    const auto a = study_hash::state_bytes(game.game()->state().left());
    const auto b = study_hash::state_bytes(game.game()->state().right());
    require(a.ok() && b.ok());
    return {std::vector<std::uint8_t>(a.data(),a.data()+a.size()),
            std::vector<std::uint8_t>(b.data(),b.data()+b.size())};
}
int main() {
    try {
        Runtime runtime; require(runtime.ready());
        int error = 0; std::uint16_t port = 0;
        auto listener = listen_loopback(0,port,error); require(listener.valid());
        std::array<Peer,2> clients;
        clients[0].socket = connect_loopback(port,error); require(clients[0].socket.valid());
        auto accepted = accept_one(listener,error); require(accepted.valid());
        Frame create; create.type = 50; create.size = 2; create.payload = {2,0};
        send_frame(clients[0].socket,create);
        AdmissionRequest host_request;
        auto response_peer = std::make_shared<Peer>(admit(std::move(accepted),host_request));
        require(host_request.route == AdmissionRoute::create);
        RoomDirectory<std::shared_ptr<Peer>,4> directory;
        auto host_owner = response_peer; // Membership keeps the same live host endpoint.
        auto pending = response_peer;
        auto made = directory.create(101,pending,[]() noexcept -> std::optional<uint32_t> { return 103; });
        require(made.status == RoomCreate::created && made.handle && !pending);
        Frame info; info.type = 52; info.size = 5;
        for (unsigned i = 0; i < 5; ++i) info.payload[i] = static_cast<std::uint8_t>(made.handle->code[i]);
        send_frame(response_peer->socket,info); // Advertise only after reservation succeeds.
        response_peer.reset(); // Directory and host_owner retain the host.
        nonblocking(clients[0].socket);
        const Frame announced = read_frame(clients[0]);
        require(announced.type == 52 && announced.size == 5);
        const std::string text(announced.payload.begin(),announced.payload.begin()+5);
        require(text == "HDAAA");

        clients[1].socket = connect_loopback(port,error); require(clients[1].socket.valid());
        auto guest_socket = accept_one(listener,error); require(guest_socket.valid());
        Frame join; join.type = 50; join.size = 7; join.payload[0] = 3; join.payload[1] = 5;
        for (unsigned i = 0; i < 5; ++i) join.payload[2+i] = announced.payload[i];
        send_frame(clients[1].socket,join);
        AdmissionRequest guest_request; auto initial_guest = admit(std::move(guest_socket),guest_request);
        require(guest_request.route == AdmissionRoute::join);
        const std::string requested(guest_request.room.begin(),guest_request.room.end());
        const auto code = parse_room_code(requested); require(bool(code));
        const auto handle = directory.find(*code); require(bool(handle));
        RoomMembership<Peer> members(handle->generation,host_owner);
        auto old_guest = std::make_shared<Peer>(std::move(initial_guest));
        const auto joined = members.join_guest(old_guest);require(joined.status == RoomJoinStatus::joined);
        const auto old_ticket = *joined.ticket;
        Frame leave;leave.type=16;send_frame(clients[1].socket,leave);
        require(read_frame(*old_guest).type==16);
        auto departed=members.leave(old_ticket);require(departed.status==RoomLeaveStatus::removed && departed.notice);
        departed.departed->socket.reset(); // State lock is already released.
        nonblocking(clients[1].socket);
        const auto eof_deadline=Clock::now()+std::chrono::seconds(2);
        for (;;) {
            std::uint8_t byte;auto r=try_receive(clients[1].socket,&byte,1);
            if(r.state==ReceiveState::eof)break;
            require(Clock::now()<eof_deadline && (r.state==ReceiveState::would_block || r.state==ReceiveState::interrupted));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        clients[1].socket=connect_loopback(port,error);require(clients[1].socket.valid());
        auto replacement_socket=accept_one(listener,error);require(replacement_socket.valid());listener.reset();
        send_frame(clients[1].socket,join);
        AdmissionRequest replacement_request;
        auto replacement=std::make_shared<Peer>(admit(std::move(replacement_socket),replacement_request));
        require(replacement_request.route==AdmissionRoute::join && replacement_request.room==guest_request.room);
        require(directory.find(*code).has_value()); // Address still reserved during leave/rejoin.
        auto new_join=members.join_guest(replacement);require(new_join.status==RoomJoinStatus::joined && *new_join.ticket!=old_ticket);
        require(members.leave(old_ticket).status==RoomLeaveStatus::stale);
        unsigned notices=0;
        auto send_notice=[&](const std::shared_ptr<Peer>& endpoint,std::size_t count) noexcept {
            ++notices;
            try {Frame state;state.type=52;state.size=1;state.payload[0]=static_cast<std::uint8_t>(count);
                 send_frame(endpoint->socket,state);return true;}
            catch (...) {return false;}
        };
        require(!members.deliver(*departed.notice,send_notice) && notices==0);
        for(std::size_t side=0;side<2;++side) {
            require(members.deliver(*members.snapshot_to(side),send_notice));
            nonblocking(clients[side].socket);
            auto state=read_frame(clients[side]);require(state.type==52 && state.size==1 && state.payload[0]==2);
        }
        auto resources=members.close();require(resources[0]==host_owner && resources[1]==replacement);
        auto& guest=*resources[1];
        auto host = directory.take(*handle); require(host && host->handle.owner == 101);
        directory.close(); require(!directory.find(*code));

        // Fixture supplies round/seed/roles; the two sockets now pass through READY.
        for (unsigned i = 0; i < 2; ++i) {
            RoundBatch batch; batch.round = 1; batch.inputs.count = 1;
            batch.inputs.masks[0] = i == 0 ? study_input::drop : 0;
            Frame ready; ready.type = kLobbyReadyType; ready.size = 1; ready.payload[0] = 1;
            Frame input; require(encode_round_input(batch,input));
            EncodedFrame er, ei; require(encode_frame(ready,er) && encode_frame(input,ei));
            std::vector<std::uint8_t> bytes(er.bytes.begin(),er.bytes.begin()+er.size);
            bytes.insert(bytes.end(),ei.bytes.begin(),ei.bytes.begin()+ei.size);
            std::size_t sent = 0;
            while (sent < bytes.size()) {
                auto r = send_some(clients[i].socket,bytes.data()+sent,bytes.size()-sent);
                require(r.status == StreamStatus::progress && r.count != 0); sent += r.count;
            }
        }
        std::array<Peer*,2> peers{host->value.get(),&guest};
        LobbyHandoff prefix;
        for (std::size_t side = 0; side < 2; ++side) {
            prefix.parsers[side] = std::move(peers[side]->parser);
            peers[side]->parser = {};
        }
        AcceptanceLobby lobby(0,2000,std::move(prefix));
        const auto start = Clock::now();
        // Supply already-read tails first, then use a bounded nonblocking receive loop.
        auto announce = [&](const LobbyStep& step) {
            require(step.state == LobbyState::waiting || step.state == LobbyState::accepted);
            if (step.notice == LobbyNotice::ready) {
                Frame notice; notice.type = kLobbyReadyType; notice.size = 1; notice.payload[0] = 1;
                send_frame(peers[step.peer]->socket,notice);
            }
        };
        for (std::size_t side = 0; side < 2 && lobby.state() == LobbyState::waiting; ++side)
            announce(lobby.feed(side,nullptr,0,0));
        while (lobby.state() == LobbyState::waiting) {
            const auto now = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count());
            require(lobby.poll(now).state == LobbyState::waiting);
            for (std::size_t side = 0; side < 2 && lobby.state() == LobbyState::waiting; ++side) {
                std::uint8_t bytes[3]; auto r = try_receive(peers[side]->socket,bytes,sizeof(bytes));
                if (r.state == ReceiveState::would_block || r.state == ReceiveState::interrupted) continue;
                require(r.state == ReceiveState::progress);
                const auto step = lobby.feed(side,bytes,r.count,now);
                announce(step);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        LobbyHandoff handoff; require(lobby.take(handoff));
        for (std::size_t side = 0; side < 2; ++side) {
            nonblocking(clients[side].socket);
            const Frame notice = read_frame(clients[side]);
            require(notice.type == kLobbyReadyType && notice.size == 1 && notice.payload[0] == 1);
        }
        ForwardDirection a_to_b(std::move(handoff.parsers[0]));
        ForwardDirection b_to_a(std::move(handoff.parsers[1]));
        std::array<ForwardDirection*,2> directions{&a_to_b,&b_to_a};
        std::array<Frame,2> delivered;
        std::array<bool,2> received{false,false};
        bool client_closed = false;
        const auto end = Clock::now()+std::chrono::seconds(2);
        // One owner visits both directions. A blocked destination pauses only
        // its source; the other direction still gets one bounded attempt.
        while (a_to_b.state() == ForwardState::running && b_to_a.state() == ForwardState::running) {
            require(Clock::now() < end);
            for (std::size_t side=0;side<2;++side) {
                auto& direction = *directions[side];
                direction.flush([&](const std::uint8_t* data,std::size_t size) noexcept {
                    // Cap progress to two bytes to exercise the saved offset.
                    return try_send(peers[1-side]->socket,data,(std::min)(size,std::size_t{2}));
                });
                if (direction.state() != ForwardState::running) break;
                if (direction.can_read()) {
                    std::uint8_t bytes[3];auto r=try_receive(peers[side]->socket,bytes,sizeof(bytes));
                    if (r.state == ReceiveState::progress) require(direction.feed(bytes,r.count));
                    else if (r.state == ReceiveState::eof) direction.eof();
                    else require(r.state == ReceiveState::would_block || r.state == ReceiveState::interrupted);
                }
                if (direction.state() != ForwardState::running) break;
            }
            for (std::size_t side=0;side<2;++side) {
                if (received[side]) continue;
                auto status=clients[side].parser.next(delivered[side]);
                if (status == ParseStatus::frame) { received[side]=true;continue; }
                require(status == ParseStatus::need_more);
                std::uint8_t bytes[3];auto r=try_receive(clients[side].socket,bytes,sizeof(bytes));
                if (r.state == ReceiveState::progress) require(clients[side].parser.append(bytes,r.count));
                else require(r.state == ReceiveState::would_block || r.state == ReceiveState::interrupted);
            }
            if (received[0] && received[1] && !client_closed) {
                clients[0].socket.reset();client_closed=true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(received[0] && received[1] && client_closed);
        require(a_to_b.state() == ForwardState::source_closed);
        b_to_a.stop(); // Pair policy: one direction ending ends this game transport.
        peers[0]->socket.reset();peers[1]->socket.reset();
        require(b_to_a.state() == ForwardState::cancelled);
        // The surviving client observes the relay's close, not an invented winner.
        for (;;) {
            std::uint8_t byte;auto r=try_receive(clients[1].socket,&byte,1);
            if (r.state == ReceiveState::eof) break;
            require(Clock::now()<end && (r.state==ReceiveState::would_block || r.state==ReceiveState::interrupted));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const Frame a = delivered[1], b = delivered[0]; // Each client received the opposite input.
        RoundBatch ia,ib; require(decode_round_input(a,ia) && decode_round_input(b,ib));
        require(ia.round == 1 && ib.round == 1 && ia.inputs.first_tick == 0 && ib.inputs.first_tick == 0 &&
                ia.inputs.count == 1 && ib.inputs.count == 1);
        auto board = study_round::Round::create_seeded(study_grid::Grid{},77); require(bool(board));
        const study_combat::Duel duel(*board,*board); RoundPlay left,right;
        require(left.prepare(1,duel,Side::host,0) && right.prepare(1,duel,Side::peer,0) && left.start() && right.start());
        Frame local_a,local_b;
        require(left.capture(ia.inputs.masks[0],local_a).input == Put::stored &&
                right.capture(ib.inputs.masks[0],local_b).input == Put::stored);
        require(left.receive(b).input == Put::stored && right.receive(a).input == Put::stored);
        require(left.advance() == Advance::advanced && right.advance() == Advance::advanced);
        require(snapshot(left) == snapshot(right));
        std::puts("TCP host create -> reserved HDAAA -> advertised code -> guest join -> versioned take");
        std::puts("guest leaves/rejoins; stale ticket/notice ignored; current presence delivered; READY/forwarding/RoundPlay bytes agree");
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
