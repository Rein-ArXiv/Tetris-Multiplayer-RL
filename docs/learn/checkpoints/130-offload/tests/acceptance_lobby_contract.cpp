#include "net/acceptance_lobby.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
using namespace study_net;
void require(bool ok) { if (!ok) throw std::runtime_error("acceptance lobby contract"); }
std::vector<std::uint8_t> wire(std::uint8_t type, std::initializer_list<std::uint8_t> payload) {
    Frame f; f.type = type; f.size = payload.size();
    std::copy(payload.begin(),payload.end(),f.payload.begin());
    EncodedFrame e; require(encode_frame(f,e));
    return {e.bytes.begin(),e.bytes.begin()+e.size};
}
LobbyStep feed(AcceptanceLobby& lobby, std::size_t side,
               const std::vector<std::uint8_t>& bytes, std::uint64_t now = 1) {
    return lobby.feed(side,bytes.data(),bytes.size(),now);
}
int main() {
    static_assert(!std::is_copy_constructible_v<AcceptanceLobby>);
    static_assert(!std::is_move_constructible_v<AcceptanceLobby>);
    static_assert(!std::is_copy_constructible_v<LobbyHandoff>);
    const auto yes = wire(17,{1}), no = wire(17,{0}), cancel = wire(11,{});
    // Every value in the one-byte domain is checked before mutation.
    for (unsigned value = 0; value < 256; ++value) {
        AcceptanceLobby lobby(0,100);
        auto r = feed(lobby,0,wire(17,{static_cast<std::uint8_t>(value)}));
        require(r.state == (value == 0 ? LobbyState::declined :
                            value == 1 ? LobbyState::waiting : LobbyState::protocol_error));
        require(lobby.ready(0) == (value == 1));
    }
    for (auto invalid : {wire(17,{}), wire(17,{1,0}), wire(11,{0}), wire(41,{1}),
                         std::vector<std::uint8_t>{0,0}}) {
        AcceptanceLobby lobby(0,100);
        require(feed(lobby,1,invalid).state == LobbyState::protocol_error);
        require(!lobby.ready(1));
    }
    // All splits across READY + complete game frame + a partial next header.
    auto game = wire(41,{9,8,7});
    auto combined = yes;
    combined.insert(combined.end(),game.begin(),game.end()); combined.push_back(3);
    for (std::size_t split = 0; split <= combined.size(); ++split) {
        for (std::size_t first = 0; first < 2; ++first) {
            AcceptanceLobby lobby(0,100);
            auto a = lobby.feed(first,combined.data(),split,1);
            auto b = lobby.feed(first,combined.data()+split,combined.size()-split,2);
            require(a.state == LobbyState::waiting && b.state == LobbyState::waiting);
            require(unsigned(a.notice == LobbyNotice::ready)+unsigned(b.notice == LobbyNotice::ready) == 1);
            require(lobby.buffered(first) == game.size()+1);
            auto r = feed(lobby,1-first,yes,3);
            require(r.state == LobbyState::accepted && r.notice == LobbyNotice::ready && r.peer == first);
            // No new lobby reads after acceptance. Pending bytes move with sockets.
            require(lobby.poll(1000).state == LobbyState::accepted);
            LobbyHandoff out; require(lobby.take(out) && !lobby.take(out));
            require(lobby.buffered(0) == 0 && lobby.buffered(1) == 0);
            LobbyHandoff moved(std::move(out));
            require(out.parsers[first].pending_bytes() == 0);
            Frame f; require(moved.parsers[first].next(f) == ParseStatus::frame &&
                             f.type == 41 && f.size == 3 && f.payload[0] == 9);
            require(moved.parsers[first].next(f) == ParseStatus::need_more &&
                    moved.parsers[first].pending_bytes() == 1);
            LobbyHandoff again; again = std::move(moved);
            require(moved.parsers[first].pending_bytes() == 0 && again.parsers[first].pending_bytes() == 1);
        }
    }
    for (auto decline : {no,cancel}) {
        for (std::size_t side = 0; side < 2; ++side) {
            AcceptanceLobby lobby(0,100); feed(lobby,1-side,yes);
            auto r = feed(lobby,side,decline,2);
            require(r.state == LobbyState::declined && r.notice == LobbyNotice::decline && r.peer == 1-side);
            require(lobby.poll(100).state == LobbyState::declined);
            LobbyHandoff out; require(!lobby.take(out));
        }
    }
    {
        AcceptanceLobby lobby(10,10); feed(lobby,0,yes,11);
        require(lobby.poll(19).state == LobbyState::waiting);
        require(lobby.feed(1,yes.data(),yes.size(),20).state == LobbyState::timed_out);
        require(!lobby.ready(1)); // Equality belongs to the deadline, not READY.
    }
    {
        constexpr auto max = std::numeric_limits<std::uint64_t>::max();
        AcceptanceLobby lobby(max-5,5);
        require(lobby.poll(max-1).state == LobbyState::waiting);
        require(lobby.poll(max).state == LobbyState::timed_out);
        AcceptanceLobby no_overflow(max-5,10);
        require(no_overflow.poll(max).state == LobbyState::waiting);
    }
    {
        AcceptanceLobby lobby(10,10);
        require(feed(lobby,0,yes,9).state == LobbyState::clock_error && !lobby.ready(0));
        require(feed(lobby,2,yes,100).state == LobbyState::invalid_side);
        require(feed(lobby,0,yes,10).state == LobbyState::waiting);
        require(lobby.eof(0,11).state == LobbyState::peer_closed);
        require(lobby.poll(0).state == LobbyState::peer_closed);
    }
    {
        AcceptanceLobby lobby(0,100); feed(lobby,0,yes); feed(lobby,1,yes,2);
        lobby.close(); LobbyHandoff out;
        require(lobby.state() == LobbyState::peer_closed && !lobby.take(out));
        AcceptanceLobby zero(0,0); require(zero.state() == LobbyState::invalid_config);
    }
    {
        AcceptanceLobby lobby(0,100); feed(lobby,0,yes);
        std::uint8_t bytes[kReceiveCapacity+1]{};
        require(lobby.feed(0,bytes,kReceiveCapacity,2).state == LobbyState::waiting);
        require(lobby.feed(0,bytes,1,3).state == LobbyState::buffer_limit);
    }
    {
        LobbyHandoff prefix;
        require(prefix.parsers[0].append(yes.data(),yes.size()));
        require(prefix.parsers[1].append(yes.data(),1));
        AcceptanceLobby lobby(0,100,std::move(prefix));
        require(prefix.parsers[0].pending_bytes() == 0 && prefix.parsers[1].pending_bytes() == 0);
        auto r = lobby.feed(0,nullptr,0,1);
        require(r.notice == LobbyNotice::ready && lobby.ready(0));
        require(lobby.feed(1,yes.data()+1,yes.size()-1,2).state == LobbyState::accepted);
    }
    // After acceptance of one side, a later CANCEL is opaque game-phase data.
    {
        AcceptanceLobby lobby(0,100); feed(lobby,0,yes); feed(lobby,0,cancel,2);
        require(feed(lobby,1,yes,3).state == LobbyState::accepted);
        LobbyHandoff out; require(lobby.take(out)); Frame f;
        require(out.parsers[0].next(f) == ParseStatus::frame && f.type == 11);
        lobby.close(); require(lobby.state() == LobbyState::handed_off);
    }
    std::puts("lobby: byte domain256, all split positions/both orders, deadline equality/overflow, EOF, limits, single handoff");
}
