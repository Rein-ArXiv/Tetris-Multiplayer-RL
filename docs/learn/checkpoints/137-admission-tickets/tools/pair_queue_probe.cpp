#include "net/pair_queue.h"
#include "net/worker_group.h"
#include "net/first_admission.h"
#include "net/round_play.h"
#include "net/send_socket.h"
#include "net/receive_socket.h"
#include "net/stream.h"
#include "simulation/state_hash.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace study_net;

using Clock = std::chrono::steady_clock;

void require(bool ok)
{
    if (!ok)
        throw std::runtime_error("pair queue probe contract");
}

struct Peer
{
    Socket socket;
    FrameParser parser;
};

bool poll_peer(std::uint64_t, Peer& peer) noexcept
{
    for (unsigned pass = 0; pass < 2; ++pass)
    {
        for (;;)
        {
            Frame f;
            auto parsed = peer.parser.next(f);
            if (parsed == ParseStatus::error)
                return false;
            if (parsed == ParseStatus::need_more)
                break;
            if (f.type == 51)
                return false; // Empty = cancel; malformed cancel also closes.
            // Other complete messages are not accepted in the queue phase.
        }
        if (pass == 1)
            return true;
        std::uint8_t bytes[16];
        auto r = try_receive(peer.socket, bytes, sizeof(bytes));
        if (r.state == ReceiveState::would_block || r.state == ReceiveState::interrupted)
            return true;
        if (r.state != ReceiveState::progress || !peer.parser.append(bytes, r.count))
            return false;
    }
    return true;
}

void append(std::vector<std::uint8_t>& out, const Frame& f)
{
    EncodedFrame e;
    require(encode_frame(f, e));
    out.insert(out.end(), e.bytes.begin(), e.bytes.begin() + e.size);
}

void send_fixture(Socket& socket, const std::vector<std::uint8_t>& bytes)
{
    std::size_t done = 0;
    while (done < bytes.size())
    {
        auto r = send_some(socket, bytes.data() + done, bytes.size() - done);
        require(r.status == StreamStatus::progress && r.count);
        done += r.count;
    }
}

Peer admit_fixture(Socket socket, std::size_t expected)
{
    // The fixture knows whether it sent5 or8 bytes. Gather with cap3 before feed
    // so the cancel is deterministically already buffered, not a TCP timing bet.
    std::array<std::uint8_t, 16> bytes{};
    std::size_t done = 0;
    require(expected <= bytes.size());
    while (done < expected)
    {
        auto r = receive_some(socket, bytes.data() + done, std::min<std::size_t>(3, expected - done));
        require(r.status == StreamStatus::progress && r.count);
        done += r.count;
    }
    auto first = *FirstAdmission::create(0, 1000);
    require(first.feed(bytes.data(), done, 1) == AdmissionState::routed);
    AdmissionHandoff handoff;
    require(first.take(handoff) && handoff.request.route == AdmissionRoute::queue);
    int error = 0;
    require(set_nonblocking(socket, true, error));
    return {std::move(socket), std::move(handoff.parser)};
}

Frame read_input(Peer& peer)
{
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    for (;;)
    {
        Frame f;
        auto parsed = peer.parser.next(f);
        if (parsed == ParseStatus::frame)
            return f;
        require(parsed == ParseStatus::need_more && Clock::now() < deadline);
        std::uint8_t bytes[16];
        auto r = try_receive(peer.socket, bytes, sizeof(bytes));
        if (r.state == ReceiveState::progress)
            require(peer.parser.append(bytes, r.count));
        else
        {
            require(r.state == ReceiveState::would_block || r.state == ReceiveState::interrupted);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

using Snapshot = std::array<std::vector<std::uint8_t>, 2>;

Snapshot snapshot(const RoundPlay& game)
{
    const auto a = study_hash::state_bytes(game.game()->state().left()), b = study_hash::state_bytes(game.game()->state().right());
    require(a.ok() && b.ok());
    return {std::vector<std::uint8_t>(a.data(), a.data() + a.size()), std::vector<std::uint8_t>(b.data(), b.data() + b.size())};
}

int main()
{
    try
    {
        Runtime runtime;
        require(runtime.ready());
        int error = 0;
        std::uint16_t port = 0;
        auto listener = listen_loopback(0, port, error);
        require(listener.valid());
        std::array<Socket, 3> clients;
        PairQueue<Peer, 8> queue;
        for (unsigned i = 0; i < 3; ++i)
        {
            clients[i] = connect_loopback(port, error);
            require(clients[i].valid());
            auto accepted = accept_one(listener, error);
            require(accepted.valid());
            Frame first;
            first.type = 50;
            first.size = 2;
            first.payload = {1, 0};
            std::vector<std::uint8_t> wire;
            append(wire, first);
            if (i == 1)
            {
                Frame cancel;
                cancel.type = 51;
                append(wire, cancel);
            }
            send_fixture(clients[i], wire);
            auto p = admit_fixture(std::move(accepted), wire.size());
            require(queue.enqueue(i + 1, p) == QueueJoin::queued && !p.socket.valid());
        }
        listener.reset();
        std::mutex mu;
        std::condition_variable cv;
        bool matched = false, verified = false;
        WorkerGroup workers{1};
        require(workers.launch([&] {
            auto pair = queue.wait_pair(poll_peer);
            require(bool(pair));
            require(pair->first.id == 1 && pair->second.id == 3);
            {
                std::lock_guard<std::mutex> lock(mu);
                matched = true;
                cv.notify_all();
            }
            // Initial round/seed/roles are supplied by this experiment, not by matching.
            const Frame a = read_input(pair->first.value), b = read_input(pair->second.value);
            RoundBatch ia, ib;
            require(decode_round_input(a, ia) && decode_round_input(b, ib));
            require(ia.round == 1 && ib.round == 1 && ia.inputs.first_tick == 0 && ib.inputs.first_tick == 0 && ia.inputs.count == 1 && ib.inputs.count == 1);
            auto board = study_round::Round::create_seeded(study_grid::Grid{}, 77);
            require(bool(board));
            RoundPlay host, peer;
            const study_combat::Duel duel(*board, *board);
            require(host.prepare(1, duel, Side::host, 0) && peer.prepare(1, duel, Side::peer, 0) && host.start() && peer.start());
            Frame local_a, local_b;
            require(host.capture(ia.inputs.masks[0], local_a).input == Put::stored && peer.capture(ib.inputs.masks[0], local_b).input == Put::stored);
            require(host.receive(b).input == Put::stored && peer.receive(a).input == Put::stored);
            require(host.advance() == Advance::advanced && peer.advance() == Advance::advanced);
            require(snapshot(host) == snapshot(peer));
            verified = true;
        }));
        workers.stop_accepting();
        bool ready = false;
        {
            std::unique_lock<std::mutex> lock(mu);
            ready = cv.wait_for(lock, std::chrono::seconds(3), [&] { return matched; });
        }
        if (!ready)
        {
            queue.close();
            workers.wait();
            require(false);
        }
        for (unsigned i : {0u, 2u})
        {
            RoundBatch batch;
            batch.round = 1;
            batch.inputs.count = 1;
            batch.inputs.masks[0] = i == 0 ? study_input::drop : 0;
            Frame input;
            require(encode_round_input(batch, input));
            std::vector<std::uint8_t> wire;
            append(wire, input);
            send_fixture(clients[i], wire);
        }
        // The pair now owns its sockets; closing the pending queue does not close it.
        queue.close();
        workers.wait();
        require(verified && workers.failed_tasks() == 0 && queue.size() == 0);
        std::puts("TCP1,2,3 admitted; buffered cancel2 removed; FIFO pair1,3 owns sockets/parser");
        std::puts("closing pending queue preserves handed-off pair; real inputs produce identical RoundPlay state bytes");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
