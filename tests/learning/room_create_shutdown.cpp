// Built by check_learning_room_code.py with one gate inserted immediately before
// handleCreate's state lock. The gate controls scheduling, not the state decision.
#include "room.h"
#include "relay.h"
#include "log.h"
#include <condition_variable>
#include <cstdio>
#include <thread>

std::mutex gate_mu;
std::condition_variable gate_cv;
bool entered = false, resume_create = false;
void room_create_gate() {
    std::unique_lock<std::mutex> lock(gate_mu);
    entered = true; gate_cv.notify_all();
    gate_cv.wait(lock, [] { return resume_create; });
}
namespace relay { void startPump(Match, meta::client::MetaClient*) {} }
struct CaptureTransport final : net::StreamTransport {
    unsigned sends = 0;
    bool closed = false;
    bool alive() const override { return !closed; }
    bool send(const void*, size_t) override { ++sends; return true; }
    bool receive(std::vector<uint8_t>&) override { return !closed; }
    void close() override { closed = true; }
};
int main() {
    relay::set_log_level(relay::LogLevel::Warn);
    relay::RoomRegistry registry;
    auto transport = std::make_shared<CaptureTransport>();
    net::TcpSocket socket; socket.transport = transport;
    auto lease = relay::PlayerSessionLease::acquire(33001);
    std::weak_ptr<relay::PlayerSessionLease> weak = lease;
    std::thread creator([&, owned = std::move(lease)]() mutable {
        registry.handleCreate(socket, 1, 33001, 0, "", "", "default", std::move(owned));
    });
    bool at_gate;
    {
        std::unique_lock<std::mutex> lock(gate_mu);
        at_gate = gate_cv.wait_for(lock, std::chrono::seconds(3), [] { return entered; });
    }
    registry.shutdown();
    { std::lock_guard<std::mutex> lock(gate_mu); resume_create = true; gate_cv.notify_all(); }
    creator.join();
    if (!at_gate || transport->sends != 0 || !transport->closed || !weak.expired()) {
        std::fprintf(stderr, "shutdown admission: gate=%d sends=%u closed=%d lease_expired=%d\n",
                     at_gate, transport->sends, transport->closed, weak.expired());
        return 1;
    }
    std::puts("room create: shutdown wins before state lock; no ROOM_INFO, connection closed, lease released");
}
