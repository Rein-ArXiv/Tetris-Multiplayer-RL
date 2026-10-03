#ifndef STUDY_NET_RELAY_COST_H
#define STUDY_NET_RELAY_COST_H

#include <cstdint>
#include <limits>

namespace study_net {

// ---------------------------------------------------------------------------
// Checked unsigned arithmetic
//
// The relay cost models below estimate microsecond totals and byte counts as
// std::uint64_t. Overflow must be detected before it happens (no wrapping), and a
// failed check must leave the caller's output untouched.
// ---------------------------------------------------------------------------

// a + b with overflow detection. On success writes the sum to out; on failure
// returns false and leaves out unchanged.
inline bool checked_cost_add(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (a > (std::numeric_limits<std::uint64_t>::max)() - b) return false;
    out = a + b;
    return true;
}

// a * b with overflow detection. A zero operand is always representable and
// succeeds with 0, even when the other operand is the maximum value. On
// failure returns false and leaves out unchanged.
inline bool checked_cost_mul(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (a == 0 || b == 0) {
        out = 0;
        return true;
    }
    if (a > (std::numeric_limits<std::uint64_t>::max)() / b) return false;
    out = a * b;
    return true;
}

// ---------------------------------------------------------------------------
// Per-direction latency estimate
//
// The four components refer to chosen application-level transfer observation points.
// They are estimates, not a TCP RTT measurement, and no assertion is made that
// a relay path is slower (or faster) than a direct path.
// ---------------------------------------------------------------------------
struct RelayDirection {
    std::uint64_t first_leg_us = 0;   // client -> relay leg
    std::uint64_t relay_wait_us = 0;  // scheduling / buffering wait at the relay
    std::uint64_t relay_work_us = 0;  // relay forwarding work
    std::uint64_t second_leg_us = 0;  // relay -> peer leg
};

// Sum the four components with checked arithmetic, accumulating in a local
// intermediate and only committing out on success.
inline bool estimate_direction(const RelayDirection& dir, std::uint64_t& out) noexcept {
    std::uint64_t total = 0;
    if (!checked_cost_add(total, dir.first_leg_us, total)) return false;
    if (!checked_cost_add(total, dir.relay_wait_us, total)) return false;
    if (!checked_cost_add(total, dir.relay_work_us, total)) return false;
    if (!checked_cost_add(total, dir.second_leg_us, total)) return false;
    out = total;
    return true;
}

// ---------------------------------------------------------------------------
// Traffic estimate
//
// Counts one fixed byte size and rate per client, two clients per match, and
// forwards the same bytes once. This is a pure accounting estimate and is not
// authority or security verification.
//
// Excluded: RSS/queue residency, socket buffer sizing, TCP/IP/link overhead, ACKs,
// retransmissions, TLS, handshake/authentication/logging and listener costs.
// ---------------------------------------------------------------------------
struct RelayTrafficPlan {
    std::uint64_t matches = 0;                       // active matches
    std::uint64_t frame_bytes = 0;                   // one frame size
    std::uint64_t frames_per_second_per_client = 0;  // frame rate per client
    std::uint64_t queued_bytes_per_connection = 0;   // queue budget per connection
};

struct RelayTraffic {
    std::uint64_t connected_sockets = 0;
    std::uint64_t ingress_bytes_per_second = 0;
    std::uint64_t egress_bytes_per_second = 0;
    std::uint64_t queue_capacity_bytes = 0;
};

// Derive traffic counters into a local candidate. All fields are only committed
// to out on full success; otherwise out is left unchanged.
inline bool estimate_traffic(const RelayTrafficPlan& plan, RelayTraffic& out) noexcept {
    RelayTraffic candidate{};

    // Two clients (connections) per match.
    {
        std::uint64_t connections = 0;
        if (!checked_cost_mul(plan.matches, 2, connections)) return false;
        candidate.connected_sockets = connections;
    }

    // Ingress/egress: matches * 2 * frame_bytes * fps, forwarded once.
    // Zero factors are handled before multiplying so a zero final result can
    // never overflow an intermediate value. Connections and queue capacity are
    // still checked even when the traffic rate is zero.
    {
        std::uint64_t traffic = 0;
        if (plan.matches != 0 && plan.frame_bytes != 0 &&
            plan.frames_per_second_per_client != 0) {
            std::uint64_t per_frame_total = 0;
            if (!checked_cost_mul(candidate.connected_sockets, plan.frame_bytes,
                                  per_frame_total))
                return false;
            if (!checked_cost_mul(per_frame_total,
                                  plan.frames_per_second_per_client, traffic))
                return false;
        }
        candidate.ingress_bytes_per_second = traffic;
        candidate.egress_bytes_per_second = traffic;
    }

    // Queue capacity: connections * queued_bytes_per_connection.
    {
        std::uint64_t capacity = 0;
        if (!checked_cost_mul(candidate.connected_sockets,
                              plan.queued_bytes_per_connection, capacity))
            return false;
        candidate.queue_capacity_bytes = capacity;
    }

    out = candidate;
    return true;
}

}  // namespace study_net

#endif  // STUDY_NET_RELAY_COST_H
