#include "net/seed_handshake.h"
#include "net/input_codec.h"
#include "net/delayed_lockstep.h"
#include "simulation/state_hash.h"
#include <charconv>
#include <cstdio>
#include <cstring>
namespace {
constexpr std::uint8_t kInputType = 20;
constexpr std::uint32_t kTicks = 32;
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
// A bounded diagnostic: stage 32 local inputs, then send two tiny frames.
// A streaming game must interleave sending and receiving instead of sending an
// unbounded history before it reads. Countdown is zero; the negotiated delay gates local consumption.
bool exchange(study_net::Connection& connection, study_net::DelayedLockstep& game,
              study_net::Side local) {
    using namespace study_net;
    for (std::uint32_t tick = 0; tick < kTicks; ++tick)
        if (game.capture(scripted(local, tick)) != Put::stored) return false;
    for (const std::uint32_t first : {16u, 0u}) {
        InputBatch batch;
        batch.first_tick = first;
        batch.count = 16;
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
    for (;;) {
        Frame frame;
        const auto received = connection.next_frame(frame);
        if (received.state == Connection::ReadState::eof) return game.next_tick() == kTicks - game.delay();
        if (received.state != Connection::ReadState::frame || frame.type != kInputType) return false;
        InputBatch batch;
        if (!decode_input_payload(frame, batch)) return false;
        // This experiment has exactly 32 produced ticks. The generic window can hold 32.
        if (batch.first_tick >= kTicks || batch.count > kTicks - batch.first_tick) return false;
        if (!accepted(game.receive_batch(batch.first_tick, batch.masks.data(), batch.count))) return false;
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
        std::puts("delay_probe listen PORT SEED DELAY | connect PORT"); return 0;
    }
    if (argc < 3) return 2;
    const bool host = std::strcmp(argv[1], "listen") == 0;
    if ((host && argc != 5) || (!host && (std::strcmp(argv[1], "connect") != 0 || argc != 3))) return 2;
    std::uint64_t port = 0, seed = 0, delay = 0;
    if (!number(argv[2],port) || port > 65535 || (!host && port == 0) || (host && (!number(argv[3],seed) || !number(argv[4],delay) || delay > 30))) return 2;
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
    const Config proposed{seed,0,static_cast<std::uint8_t>(delay)};
    const auto negotiation = host ? negotiate_host(connection,proposed,agreed) : negotiate_peer(connection,agreed);
    if (negotiation != HandshakeResult::ready || agreed.countdown_ticks != 0) return 1;
    const auto round = study_round::Round::create_seeded(study_grid::Grid{},agreed.seed);
    if (!round) return 1;
    auto paced = DelayedLockstep::create(study_combat::Duel{*round,*round},host ? Side::host : Side::peer,agreed.input_delay);
    if (!paced) return 1;
    auto& game = *paced;
    const bool ok = exchange(connection,game,host ? Side::host : Side::peer);
    connection.close();
    if (!ok) {
        std::printf("FAILED next=%llu\n",static_cast<unsigned long long>(game.next_tick())); return 1;
    }
    const auto left = study_hash::state_hash(game.state().left());
    const auto right = study_hash::state_hash(game.state().right());
    if (!left || !right) return 1;
    std::printf("DONE role=%s ticks=%llu left=%llu right=%llu delay=%u pending=%llu\n",host ? "host" : "peer",
        static_cast<unsigned long long>(game.next_tick()),static_cast<unsigned long long>(*left),static_cast<unsigned long long>(*right),unsigned(game.delay()),static_cast<unsigned long long>(game.next_capture_tick()-game.next_tick()));
}
