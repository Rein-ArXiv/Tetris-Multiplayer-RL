#include "net/buffered_relay.h"
#include "net/reactor_choice.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
void small_send_buffer(Socket& socket) {
    const int bytes = 4096;
#ifdef _WIN32
    require(setsockopt(static_cast<SOCKET>(socket.native()), SOL_SOCKET, SO_SNDBUF,
        reinterpret_cast<const char*>(&bytes), sizeof(bytes)) == 0, "send buffer");
#else
    require(setsockopt(socket.native(), SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes)) == 0, "send buffer");
#endif
}
int main(int argc, char** argv) {
    Runtime runtime;
    if (!runtime.ready()) return 2;
    try {
        bool epoll = false, stall = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--epoll") epoll = true;
            else if (arg == "--stall") stall = true;
            else throw std::runtime_error("unknown argument");
        }
        auto a = pair(), b = pair();
        small_send_buffer(b.receiver);
        BufferedRelay relay(choose_reactor(epoll));
        relay.attach(std::move(a.receiver), std::move(b.receiver));
        std::size_t submitted = 0, received = 0;
        const auto stop = BufferedRelay::Clock::now() + 12s;
        auto offer = [&](std::size_t target) {
            if (submitted >= target) return;
            std::uint8_t bytes[16];
            const auto size = (std::min)(sizeof(bytes), target - submitted);
            for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>((submitted + i) % 251);
            const auto result = try_send(a.sender, bytes, size);
            require(result.state != SendState::error, "source send");
            if (result.state == SendState::progress) submitted += result.count;
        };
        // B deliberately does not read; queue growth must pause source A.
        auto paused_since = BufferedRelay::Clock::time_point{};
        while (!relay.closed() && BufferedRelay::Clock::now() < stop) {
            offer(8 * 1024 * 1024);
            relay.tick(0);
            const auto now = BufferedRelay::Clock::now();
            if (!relay.paused(0)) paused_since = {};
            else if (paused_since == BufferedRelay::Clock::time_point{}) paused_since = now;
            else if (now - paused_since >= 100ms) break;
        }
        require(relay.paused(0) && relay.pending() <= 8192, "slow reader pauses source");
        if (stall) {
            while (!relay.closed() && BufferedRelay::Clock::now() < stop) {
                offer(8 * 1024 * 1024);
                relay.tick();
            }
            require(relay.closed() && relay.stalls() == 1 && relay.pending() == 0, "no progress expires pair");
            std::puts("STALL: queue deadline closed pair and returned byte reservations");
            return 0;
        }
        const std::uint8_t reverse = 233;
        require(send_bounded(b.sender, &reverse, 1, BufferedRelay::Clock::now() + 1s).outcome == SendOutcome::complete, "reverse send");
        bool reversed = false;
        while (!reversed && BufferedRelay::Clock::now() < stop) {
            relay.tick();
            std::uint8_t value = 0;
            const auto read = try_receive(a.sender, &value, 1);
            if (read.state == ReceiveState::progress) { require(value == reverse, "reverse byte"); reversed = true; }
            else require(read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted, "reverse receive");
        }
        require(reversed && !relay.closed() && relay.paused(0), "source Read pause preserves reverse Write");
        const auto target = submitted + 4096;
        while (received < target && !relay.closed() && BufferedRelay::Clock::now() < stop) {
            offer(target);
            relay.tick(0);
            std::uint8_t bytes[4096];
            const auto read = try_receive(b.sender, bytes, sizeof(bytes));
            if (read.state == ReceiveState::progress) {
                for (std::size_t i = 0; i < read.count; ++i)
                    require(bytes[i] == static_cast<std::uint8_t>((received + i) % 251), "FIFO bytes");
                received += read.count;
            } else require(read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted, "destination receive");
        }
        require(received == target && submitted == target && relay.resumes() > 0 && !relay.closed(), "resume and exact transfer");
        relay.close();
        require(relay.pending() == 0, "all reservations returned");
        std::printf("FLOW: %zu ordered bytes; pauses=%zu resumes=%zu; reverse byte delivered while source Read paused\n", received, relay.pauses(), relay.resumes());
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
