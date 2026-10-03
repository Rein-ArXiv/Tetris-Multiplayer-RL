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
        auto pending = response_peer; // A short-lived owning alias for the response.
        auto made = directory.create(101,pending,[]() noexcept -> std::optional<uint32_t> { return 103; });
        require(made.status == RoomCreate::created && made.handle && !pending);
        Frame info; info.type = 52; info.size = 5;
        for (unsigned i = 0; i < 5; ++i) info.payload[i] = static_cast<std::uint8_t>(made.handle->code[i]);
        send_frame(response_peer->socket,info); // Advertise only after reservation succeeds.
        response_peer.reset(); // Directory now holds the sole shared_ptr to the host Peer.
        nonblocking(clients[0].socket);
        const Frame announced = read_frame(clients[0]);
        require(announced.type == 52 && announced.size == 5);
        const std::string text(announced.payload.begin(),announced.payload.begin()+5);
        require(text == "HDAAA");

        clients[1].socket = connect_loopback(port,error); require(clients[1].socket.valid());
        auto guest_socket = accept_one(listener,error); require(guest_socket.valid()); listener.reset();
        Frame join; join.type = 50; join.size = 7; join.payload[0] = 3; join.payload[1] = 5;
        for (unsigned i = 0; i < 5; ++i) join.payload[2+i] = announced.payload[i];
        send_frame(clients[1].socket,join);
        AdmissionRequest guest_request; auto guest = admit(std::move(guest_socket),guest_request);
        require(guest_request.route == AdmissionRoute::join);
        const std::string requested(guest_request.room.begin(),guest_request.room.end());
        const auto code = parse_room_code(requested); require(bool(code));
        const auto handle = directory.find(*code); require(bool(handle));
        auto host = directory.take(*handle); require(host && host->handle.owner == 101);
        directory.close(); require(!directory.find(*code));

        // Round/seed/roles are supplied by the fixture; no READY protocol here.
        for (unsigned i = 0; i < 2; ++i) {
            RoundBatch batch; batch.round = 1; batch.inputs.count = 1;
            batch.inputs.masks[0] = i == 0 ? study_input::drop : 0;
            Frame input; require(encode_round_input(batch,input)); send_frame(clients[i].socket,input);
        }
        const Frame a = read_frame(*host->value), b = read_frame(guest);
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
        std::puts("directory close preserves handed-off host; real RoundPlay state bytes agree");
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
