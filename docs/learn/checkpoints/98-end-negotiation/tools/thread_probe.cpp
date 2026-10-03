#include "net/thread_link.h"
#include "net/hash_protocol.h"
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <utility>

int main(int argc, char** argv) {
    using namespace study_net;
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::puts("thread_probe listen PORT | connect PORT"); return 0;
    }
    if (argc != 3) return 2;
    unsigned port = 0;
    const auto end = argv[2] + std::strlen(argv[2]);
    const auto parsed = std::from_chars(argv[2], end, port);
    if (parsed.ec != std::errc{} || parsed.ptr != end || port > 65535) return 2;
    const bool host = std::strcmp(argv[1], "listen") == 0;
    if (!host && (std::strcmp(argv[1], "connect") != 0 || port == 0)) return 2;
    Runtime runtime;
    if (!runtime.ready()) return 1;
    int error = 0;
    Socket socket;
    if (host) {
        std::uint16_t bound = 0;
        auto listener = listen_loopback(static_cast<std::uint16_t>(port), bound, error);
        if (!listener.valid()) return 1;
        std::printf("LISTEN %u\n", unsigned(bound)); std::fflush(stdout);
        socket = accept_one(listener, error); // Startup remains a synchronous phase.
    } else socket = connect_loopback(static_cast<std::uint16_t>(port), error);
    if (!socket.valid()) return 1;
    ThreadLink link(std::move(socket));
    // Controlled transport exercise. These known values are not game hashes.
    unsigned received = 0;
    for (unsigned i = 0; i < 4; ++i) {
        Frame frame;
        if (!encode_stamp({i * 4u, 11u + i, 29u + i}, frame) ||
            link.send(frame) != QueuePut::stored) return 1;
    }
    link.finish_sending();
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < limit) {
        Frame frame;
        QueueGet got;
        while ((got = link.receive(frame)) == QueueGet::item) {
            StateStamp stamp;
            if (received >= 4 || !decode_stamp(frame, stamp) ||
                stamp.tick != received * 4 || stamp.host_hash != 11u + received ||
                stamp.peer_hash != 29u + received) return 1;
            ++received;
        }
        // Close on the inbound queue means drained. The terminal report is
        // published separately and may arrive just after this observation.
        const auto result = link.report();
        if (result && got == QueueGet::closed) {
            std::printf("DONE frames=%u end=%d\n", received, int(result->end));
            return result->end == LinkEnd::complete && received == 4 ? 0 : 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::puts("TIMEOUT"); return 1; // Destructor requests cancellation and joins.
}
