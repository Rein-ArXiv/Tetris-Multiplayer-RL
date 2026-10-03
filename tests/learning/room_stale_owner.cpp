// Test-only header helpers replace a room while its old reader owns a socket.
#include "room.h"
#include "relay.h"
#include "log.h"
#include "net/framing.h"
#include <cstdio>
#include <functional>
namespace relay { void startPump(Match,meta::client::MetaClient*) {} }
struct Transport final : net::StreamTransport {
    bool closed=false; unsigned reads=0,sends=0;
    std::function<bool(std::vector<uint8_t>&)> on_read;
    bool alive() const override { return !closed; }
    bool send(const void*,size_t) override {++sends;return !closed;}
    bool receive(std::vector<uint8_t>& out) override {++reads;return on_read ? on_read(out) : false;}
    void close() override {closed=true;}
};
net::TcpSocket socket(const std::shared_ptr<Transport>& t) { net::TcpSocket s;s.transport=t;return s; }
int main(int argc,char**) {
    relay::set_log_level(relay::LogLevel::Warn);
    relay::RoomRegistry registry;
    auto old=std::make_shared<Transport>(), fresh=std::make_shared<Transport>(), peer=std::make_shared<Transport>();
    auto a=socket(old), b=socket(fresh), c=socket(peer);
    registry.study_seed(a,{});
    if (argc>1) {
        registry.study_seed(b,c); // Code reused before the old reader even enters.
    } else {
        old->on_read=[&](std::vector<uint8_t>& out) {
            registry.study_seed(b,c); // An old read completes after replacement.
            const auto ready=net::build_frame(net::MsgType::READY,{1});
            const auto leave=net::build_frame(net::MsgType::ROOM_LEAVE,{});
            out.insert(out.end(),ready.begin(),ready.end());out.insert(out.end(),leave.begin(),leave.end());
            return true;
        };
    }
    registry.study_run(true,a);
    const bool intact=registry.study_intact(b,c);
    if (!intact || !old->closed || fresh->closed || peer->closed || fresh->reads || peer->sends) {
        std::fprintf(stderr,"stale reader: intact=%d oldclosed=%d freshclosed=%d peerclosed=%d freshreads=%u peersends=%u\n",
                     intact,old->closed,fresh->closed,peer->closed,fresh->reads,peer->sends);return 1;
    }
    registry.shutdown();
    std::puts("stale room reader: replacement presence/readiness/socket/notice preserved; old socket closed");
}
