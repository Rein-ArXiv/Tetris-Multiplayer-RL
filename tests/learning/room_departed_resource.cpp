// Leaving a guest slot must not keep its transport alive in the surviving room.
#include "room.h"
#include "relay.h"
#include "log.h"
#include <cstdio>
namespace relay { void startPump(Match,meta::client::MetaClient*) {} }
struct Transport final : net::StreamTransport {
    bool alive() const override {return true;}
    bool send(const void*,size_t) override {return true;}
    bool receive(std::vector<uint8_t>&) override {return false;}
    void close() override {}
};
int main() {
    relay::set_log_level(relay::LogLevel::Warn);
    relay::RoomRegistry registry;
    net::TcpSocket host,guest;host.transport=std::make_shared<Transport>();guest.transport=std::make_shared<Transport>();
    std::weak_ptr<net::StreamTransport> weak=guest.transport;
    registry.study_seed(host,guest);
    registry.study_run(false,guest);
    guest={};
    const bool released=weak.expired();
    registry.shutdown();
    if (!released) {std::fputs("departed guest transport retained by empty room slot\n",stderr);return 1;}
    std::puts("departed guest transport released while host room survives");
}
