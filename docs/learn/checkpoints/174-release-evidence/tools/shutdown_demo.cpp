#include "net/stop_gate.h"
#include "net/thread_link.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include "net/stream.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
static_assert(std::atomic<bool>::is_always_lock_free,
              "signal handler requires lock-free atomic<bool>");
std::atomic<bool> stopping{false};
void on_signal(int) { stopping.store(true, std::memory_order_relaxed); }

int run(bool self_stop) {
    using namespace study_net;
    Runtime runtime; // Last networking object to be destroyed.
    if (!runtime.ready()) return 1;
    int error = 0;
    std::uint16_t port = 0;
    auto listener = listen_loopback(0, port, error);
    if (!listener.valid()) return 1;
    auto peer = connect_loopback(port, error);
    if (!peer.valid()) return 1;
    auto accepted = accept_one(listener, error);
    if (!accepted.valid()) return 1;
    listener.reset(); // This bounded exercise admits one local connection.
    if (!set_nonblocking(peer, true, error)) return 1;

    StopGate gate;
    WorkerGroup workers(1); // Exercise capacity; not a production server limit.
    ShutdownDrain drain(workers, gate); // Also drains on early return/exception.
    auto link = std::make_unique<ThreadLink>(std::move(accepted));
    if (!workers.launch([owned = std::move(link), &gate] { gate.wait(); })) return 1;
    std::puts("READY");
    std::fflush(stdout);
    if (self_stop) stopping.store(true, std::memory_order_relaxed);
    while (!stopping.load(std::memory_order_relaxed))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    drain.finish();
    if (workers.launch([] {})) return 2; // Admission remains closed.
    std::puts("TASKS_DRAINED");
    // The capture owned ThreadLink; its destructor joined its I/O thread.
    // EOF is an observation here, not an application-level delivery ACK.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        std::uint8_t byte = 0;
        const auto result = try_receive(peer, &byte, 1);
        if (result.state == ReceiveState::eof) {
            std::puts("PEER_EOF");
            return 0;
        }
        if (result.state != ReceiveState::would_block &&
            result.state != ReceiveState::interrupted) return 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 4;
}
}

int main(int argc, char** argv) {
    const bool self_stop = argc == 2 && std::strcmp(argv[1], "--self-stop") == 0;
    if (argc != 1 && !self_stop) return 64;
    if (std::signal(SIGINT, on_signal) == SIG_ERR ||
        std::signal(SIGTERM, on_signal) == SIG_ERR) return 1;
#ifdef _WIN32
    if (std::signal(SIGBREAK, on_signal) == SIG_ERR) return 1;
#endif
    try { return run(self_stop); }
    catch (...) {
        std::fputs("shutdown exercise failed\n", stderr);
        return 1;
    }
}
