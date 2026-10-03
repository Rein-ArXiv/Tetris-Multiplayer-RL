// A scheduling hook changes the room version after guest publication, before
// the actual entered check. Test-only header helpers seed/observe that state.
#include "room.h"
#include "relay.h"
#include "log.h"
#include <cstdio>
namespace relay { void startPump(Match, meta::client::MetaClient*) {} }
relay::RoomRegistry* active_registry = nullptr;
void room_join_gate() { active_registry->study_change_version(); }
struct Transport final : net::StreamTransport {
    unsigned sends = 0; bool closed = false;
    bool alive() const override { return !closed; }
    bool send(const void*, size_t) override { ++sends; return true; }
    bool receive(std::vector<uint8_t>&) override { return !closed; }
    void close() override { closed = true; }
};
int main() {
    relay::set_log_level(relay::LogLevel::Warn);
    relay::RoomRegistry registry; active_registry = &registry;
    auto host = std::make_shared<Transport>(),guest = std::make_shared<Transport>();
    net::TcpSocket a,b; a.transport = host; b.transport = guest;
    registry.study_seed_room(a);
    auto lease = relay::PlayerSessionLease::acquire(33002);
    std::weak_ptr<relay::PlayerSessionLease> weak = lease;
    registry.handleJoin("AAAAA",b,2,33002,0,"","","default",std::move(lease));
    if (registry.study_room_count() || !guest->closed || !weak.expired() || guest->sends) {
        std::fprintf(stderr,"abandoned guest: rooms=%zu closed=%d lease_expired=%d sends=%u\n",
            registry.study_room_count(),guest->closed,weak.expired(),guest->sends);
        return 1;
    }
    registry.shutdown();
    std::puts("room join: stale initial notice releases published guest without starting a reader");
}
