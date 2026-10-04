#include "net/seed_handshake.h"
#include "net/input_codec.h"
#include "net/lockstep.h"
#include "simulation/state_hash.h"
#include <charconv>
#include <cstdio>
#include <cstring>
namespace {
constexpr std::uint8_t kInputType = 20;
constexpr std::uint32_t kTicks = 12;
bool number(const char* s, std::uint64_t& out) {
    const char* end = s + std::strlen(s);
    const auto r = std::from_chars(s, end, out);
    return r.ec == std::errc{} && r.ptr == end;
}
std::uint8_t scripted(study_net::Side side, std::uint32_t tick) {
    if (tick % 3 == 2) return 0;
    if (side == study_net::Side::host)
        return tick % 3 == 0 ? study_input::left : study_input::rotate;
    return tick % 3 == 0 ? study_input::right : study_input::down;
}
bool accepted(study_net::Put result) {
    return result == study_net::Put::stored || result == study_net::Put::duplicate;
}
// A bounded diagnostic: stage twelve local inputs, then send two tiny frames.
// A streaming game must interleave sending and receiving instead of sending an
// unbounded history before it reads. No countdown or extra delay is applied here.
bool exchange(study_net::Connection& connection, study_net::Lockstep& game,
              study_net::Side local) {
    using namespace study_net;
    for (std::uint32_t tick = 0; tick < kTicks; ++tick)
        if (game.submit(local, tick, scripted(local, tick)) != Put::stored) return false;
    for (const std::uint32_t first : {6u, 0u}) {
        InputBatch batch;
        batch.first_tick = first;
        batch.count = 6;
        for (std::size_t i = 0; i < batch.count; ++i)
            batch.masks[i] = scripted(local, first + static_cast<std::uint32_t>(i));
        Frame frame;
        if (!encode_input_payload(batch, frame)) return false;
        frame.type = kInputType;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        if (connection.send_frame(frame, deadline).outcome != SendOutcome::complete) return false;
    }
    int error = 0;
    if (!connection.finish_sending(error)) return false;
    const Side remote = local == Side::host ? Side::peer : Side::host;
    for (;;) {
        Frame frame;
        const auto received = connection.next_frame(frame);
        if (received.state == Connection::ReadState::eof) return game.next_tick() == kTicks;
        if (received.state != Connection::ReadState::frame || frame.type != kInputType) return false;
        InputBatch batch;
        if (!decode_input_payload(frame, batch)) return false;
        // This experiment has exactly twelve ticks. The generic window can hold 32.
        if (batch.first_tick >= kTicks || batch.count > kTicks - batch.first_tick) return false;
        if (!accepted(game.submit_batch(remote, batch.first_tick, batch.masks.data(), batch.count))) return false;
        for (;;) {
            const auto result = game.advance();
            if (result == Advance::advanced) continue;
            if (result != Advance::waiting) return false;
            std::printf("WAIT next=%llu\n",static_cast<unsigned long long>(game.next_tick()));
            break;
        }
    }
}
}
int main(int argc, char** argv) {
    using namespace study_net;
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::puts("input_probe listen PORT SEED | connect PORT"); return 0;
    }
    if (argc < 3) return 2;
    const bool host = std::strcmp(argv[1], "listen") == 0;
    if ((host && argc != 4) || (!host && (std::strcmp(argv[1], "connect") != 0 || argc != 3))) return 2;
    std::uint64_t port = 0, seed = 0;
    if (!number(argv[2],port) || port > 65535 || (!host && port == 0) || (host && !number(argv[3],seed))) return 2;
    Runtime runtime;
    if (!runtime.ready()) return 1;
    Connection connection;
    int error = 0;
    if (host) {
        std::uint16_t bound = 0;
        auto listener = listen_loopback(static_cast<std::uint16_t>(port),bound,error);
        if (!listener.valid()) return 1;
        std::printf("LISTEN %u\n",unsigned(bound)); std::fflush(stdout);
        auto socket = accept_one(listener,error);
        if (!connection.adopt(socket)) return 1;
    } else if (connection.start(static_cast<std::uint16_t>(port)).state != Connection::StartState::started) return 1;
    Config agreed;
    const Config proposed{seed,0,0};
    const auto negotiation = host ? negotiate_host(connection,proposed,agreed) : negotiate_peer(connection,agreed);
    if (negotiation != HandshakeResult::ready || agreed.countdown_ticks != 0 || agreed.input_delay != 0) return 1;
    const auto round = study_round::Round::create_seeded(study_grid::Grid{},agreed.seed);
    if (!round) return 1;
    Lockstep game{study_combat::Duel{*round,*round}};
    const bool ok = exchange(connection,game,host ? Side::host : Side::peer);
    connection.close();
    if (!ok) {
        std::printf("FAILED next=%llu\n",static_cast<unsigned long long>(game.next_tick())); return 1;
    }
    const auto left = study_hash::state_hash(game.state().left());
    const auto right = study_hash::state_hash(game.state().right());
    if (!left || !right) return 1;
    std::printf("DONE role=%s ticks=%llu left=%llu right=%llu\n",host ? "host" : "peer",
        static_cast<unsigned long long>(game.next_tick()),static_cast<unsigned long long>(*left),static_cast<unsigned long long>(*right));
}
