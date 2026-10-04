#include "net/stop_gate.h"
#include <atomic>
#include <cstdio>
#include <memory>
#include <stdexcept>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "failed at %d\n", __LINE__); return 1; } } while (false)
struct Cleanup {
    std::atomic<bool>* done;
    ~Cleanup() { done->store(true); }
};
int main() {
    study_net::StopGate early;
    early.request_stop();
    early.request_stop();
    early.wait(); // Must not wait for a future notification.
    std::atomic<bool> destroyed{false};
    try {
        study_net::StopGate gate;
        study_net::WorkerGroup group(1);
        study_net::ShutdownDrain drain(group, gate);
        auto owned=std::make_unique<Cleanup>();owned->done=&destroyed;
        CHECK(group.launch([capture=std::move(owned), &gate] { gate.wait(); }));
        throw std::runtime_error("exercise exception");
    } catch (const std::runtime_error&) {}
    CHECK(destroyed.load()); // Guard woke the task and waited for its capture.
    std::puts("sticky stop and exception-path capture drain passed");
}
