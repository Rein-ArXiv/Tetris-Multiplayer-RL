#include "net/connection_policy.h"
#include <cstdio>
#include <stdexcept>
using namespace study_net;
void check(bool value) { if (!value) throw std::runtime_error("connection policy"); }
int main() {
    try {
        const Endpoint api{Transport::https, "api.example.test", 443};
        const Endpoint game{Transport::wss, "game.example.test", 8443};
        for (int mode = 0; mode <= 2; ++mode) {
            for (int a = 0; a <= 4; ++a) for (int g = 0; g <= 4; ++g) {
                Endpoint x = api, y = game;
                x.transport = static_cast<Transport>(a); y.transport = static_cast<Transport>(g);
                const auto plan = plan_connections(static_cast<Deployment>(mode), x, y);
                check(bool(plan) == (mode < 2 && a == 1 && g == 3));
                if (plan) check(plan->api.host == api.host && plan->game.host == game.host && plan->game.port == 8443);
            }
        }
        for (const auto* host : {"127.0.0.1", "::1"}) {
            Endpoint localApi{Transport::http, host, 8080}, localGame{Transport::tcp, host, 7777};
            check(bool(plan_connections(Deployment::local_development, localApi, localGame)));
            check(!plan_connections(Deployment::public_network, localApi, localGame));
            check(bool(plan_connections(Deployment::local_development, api, localGame)));
            check(bool(plan_connections(Deployment::local_development, localApi, game)));
        }
        for (const auto* host : {"localhost", "127.0.0.1.evil", "127.1", "192.168.1.2"}) {
            check(!plan_connections(Deployment::local_development, {Transport::http, host, 8080}, game));
            check(!plan_connections(Deployment::local_development, api, {Transport::tcp, host, 7777}));
        }
        for (int byte = 0; byte <= 255; ++byte) {
            if (byte > 32 && byte < 127 && std::string("/@?#%\\").find(char(byte)) == std::string::npos) continue;
            auto bad = api; bad.host.push_back(static_cast<char>(byte));
            check(!plan_connections(Deployment::public_network, bad, game));
        }
        auto bad = api; bad.port = 0; check(!plan_connections(Deployment::public_network, bad, game));
        bad = api; bad.host.clear(); check(!plan_connections(Deployment::public_network, bad, game));
        bad.host.assign(254, 'a'); check(!plan_connections(Deployment::public_network, bad, game));
        std::puts("Two independent endpoints: role, transport, mode, loopback and byte-policy contracts passed");
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
