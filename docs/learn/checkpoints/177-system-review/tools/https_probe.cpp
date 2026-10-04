#include "net/connection_policy.h"
#include "net/tls_identity.h"
#include "httplib.h"
#include <charconv>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    unsigned port = 0;
    const std::string text = argv[2];
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        port == 0 || port > 65535) return 2;
    using namespace study_net;
    const auto plan = plan_connections(Deployment::public_network,
        {Transport::https, argv[1], static_cast<std::uint16_t>(port)},
        {Transport::wss, "game.example.test", 8443});
    if (!plan) return 3;
    // This probe opens the API connection only. The game endpoint stays a plan.
    const auto& endpoint = plan->api;
    httplib::SSLClient api(endpoint.host, endpoint.port);
    api.enable_server_certificate_verification(true);
    api.set_server_certificate_verifier([host = endpoint.host](SSL* ssl) {
        return tls_identity::verified_peer(ssl, host);
    });
    if (std::string(argv[3]) != "-") api.set_ca_cert_path(argv[3]);
    api.set_follow_location(false);
    api.set_connection_timeout(1, 0);
    api.set_read_timeout(1, 0);
    api.set_write_timeout(1, 0);
    const auto result = api.Get("/healthz");
    if (!result) { std::puts("TLS or transport failed"); return 4; }
    if (result->status != 200 || result->body != "study-meta-ready") {
        std::puts("HTTP response rejected"); return 5;
    }
    std::puts("HTTPS: trusted peer, expected identity, valid response");
}
