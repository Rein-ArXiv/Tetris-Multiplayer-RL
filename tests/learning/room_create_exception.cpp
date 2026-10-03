// The checker adds only study_room_count() to a private copy of room.h.
// Allocation failures occur in the actual handleCreate implementation.
#include "room.h"
#include "relay.h"
#include "log.h"
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <new>

std::atomic<int> allocations_until_failure{-1};
void* operator new(std::size_t n) {
    int left = allocations_until_failure.load();
    if (left >= 0 && allocations_until_failure.fetch_sub(1) == 0) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace relay { void startPump(Match, meta::client::MetaClient*) {} }
struct EofTransport final : net::StreamTransport {
    bool alive() const override { return true; }
    bool send(const void*, size_t) override { return true; }
    bool receive(std::vector<uint8_t>&) override { return false; }
    void close() override {}
};
int main() {
    relay::set_log_level(relay::LogLevel::Warn);
    const std::string name(256,'N'),token(256,'T'),icon(256,'I');
    unsigned failures = 0;
    for (int fail_at = 0; fail_at < 8; ++fail_at) {
        relay::RoomRegistry registry;
        net::TcpSocket socket; socket.transport = std::make_shared<EofTransport>();
        allocations_until_failure = fail_at;
        try { registry.handleCreate(socket,1,0,0,name,token,icon,{}); }
        catch (const std::bad_alloc&) { ++failures; }
        allocations_until_failure = -1;
        if (registry.study_room_count() != 0) {
            std::fprintf(stderr,"allocation failure %d left a published room without roomLoop\n",fail_at);
            return 1;
        }
        registry.shutdown();
    }
    if (failures < 3) return 1;
    std::puts("room create: staged Entry leaves no published room after allocating field/map failures");
}
