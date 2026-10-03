#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace study_net {

// Transport for an already-parsed endpoint; never parsed here.
enum class Transport { http, https, tcp, wss };

// Target deployment environment.
enum class Deployment { public_network, local_development };

// A parsed endpoint. host is a DNS name, IPv4 address, or an unbracketed
// IPv6 literal; this header performs no DNS/IP syntax validation.
struct Endpoint {
    Transport transport;
    std::string host;
    std::uint16_t port;
};

// The two endpoints a session should use.
struct ConnectionPlan {
    Endpoint api;
    Endpoint game;
};

// True when host is non-empty, at most 253 bytes, and free of bytes that
// are control, space, non-ASCII, or the separators / @ ? # % backslash.
inline bool has_safe_host_text(const std::string& host) {
    if (host.empty() || host.size() > 253) {
        return false;
    }
    for (char ch : host) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte <= 32 || byte >= 127) {
            return false;
        }
        switch (ch) {
            case '/':
            case '@':
            case '?':
            case '#':
            case '%':
            case '\\':
                return false;
            default:
                break;
        }
    }
    return true;
}

// True only for the exact loopback literals 127.0.0.1 or ::1;
// localhost and other aliases are intentionally rejected.
inline bool is_loopback_literal(const std::string& host) {
    return host == "127.0.0.1" || host == "::1";
}

// Validate both parsed endpoints and return copies in a ConnectionPlan.
// Rejects an invalid Deployment, an API transport other than http/https,
// a game transport other than tcp/wss, malformed/missing hosts, and a
// zero port. public_network requires https + wss; local_development
// permits secure endpoints anywhere but requires each insecure endpoint
// to use the exact loopback literal host. No resolution, rewriting,
// upgrade/downgrade, credential handling, I/O, or fallback is performed.
// Returns nullopt on any failure.
inline std::optional<ConnectionPlan> plan_connections(Deployment mode,
                                                      const Endpoint& api,
                                                      const Endpoint& game) {
    // Reject an out-of-range Deployment value.
    if (mode != Deployment::public_network && mode != Deployment::local_development) {
        return std::nullopt;
    }

    // API must be http or https; game must be tcp or wss.
    const bool apiSecure = api.transport == Transport::https;
    const bool apiInsecure = api.transport == Transport::http;
    const bool gameSecure = game.transport == Transport::wss;
    const bool gameInsecure = game.transport == Transport::tcp;
    if (!apiSecure && !apiInsecure) {
        return std::nullopt;
    }
    if (!gameSecure && !gameInsecure) {
        return std::nullopt;
    }

    // Both parsed hosts must pass the byte filter and both ports must be non-zero.
    if (!has_safe_host_text(api.host) || !has_safe_host_text(game.host)) {
        return std::nullopt;
    }
    if (api.port == 0 || game.port == 0) {
        return std::nullopt;
    }

    if (mode == Deployment::public_network) {
        // Public network forbids every insecure transport.
        if (!apiSecure || !gameSecure) {
            return std::nullopt;
        }
    } else {
        // Each insecure endpoint must use the exact loopback literal.
        if (apiInsecure && !is_loopback_literal(api.host)) {
            return std::nullopt;
        }
        if (gameInsecure && !is_loopback_literal(game.host)) {
            return std::nullopt;
        }
    }

    return ConnectionPlan{api, game};
}

}  // namespace study_net
