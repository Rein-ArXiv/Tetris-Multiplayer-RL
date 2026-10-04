#ifndef STUDY_NET_INPUT_AUTHORITY_H
#define STUDY_NET_INPUT_AUTHORITY_H

#include <cstdint>
#include <stdexcept>

#include "net/round_protocol.h"
#include "net/tick_inputs.h"

namespace study_net {

// Distinguish the boundary that rejected a request from successful storage.
enum class InputDecision {
    stored,
    duplicate,
    inactive,
    unauthenticated,
    not_participant,
    forbidden_type,
    malformed,
    wrong_round,
    conflict,
    stale,
    too_far,
    exhausted
};

// Single-owner, server-side gate for round input frames.
//
// The authority binds one round to one host and one peer identifier. Those
// identifiers and the authenticated actor are supplied by trusted admission
// or socket binding and are never read from a client frame. This class does
// no authentication, awards no results, owns no timers, and performs no I/O;
// it only validates frame shape, authorship, round and window placement before
// handing the batch to the bounded TickInputs window.
class InputAuthority {
public:
    InputAuthority(std::uint64_t round, std::uint64_t host, std::uint64_t peer)
        : round_(round), host_(host), peer_(peer) {
        if (round == 0 || host == 0 || peer == 0 || host == peer) {
            throw std::invalid_argument("InputAuthority: invalid round or participants");
        }
    }

    InputAuthority(const InputAuthority&) = delete;
    InputAuthority& operator=(const InputAuthority&) = delete;

    // Opens the authority exactly once; fails while active or after close().
    bool start() noexcept {
        if (active_ || ended_) {
            return false;
        }
        active_ = true;
        return true;
    }

    // Permanently deactivates the authority. Idempotent and final.
    void close() noexcept {
        active_ = false;
        ended_ = true;
    }

    // Validates and stores one frame batch for the given authenticated actor.
    // Rejected frames leave the input window untouched; duplicates are
    // idempotent.
    InputDecision submit(std::uint64_t authenticated_actor, const Frame& frame) noexcept {
        if (!active_) {
            return InputDecision::inactive;
        }
        if (authenticated_actor == 0) {
            return InputDecision::unauthenticated;
        }

        Side side;
        if (authenticated_actor == host_) {
            side = Side::host;
        } else if (authenticated_actor == peer_) {
            side = Side::peer;
        } else {
            return InputDecision::not_participant;
        }

        if (frame.type != kRoundInputType) {
            return InputDecision::forbidden_type;
        }

        RoundBatch batch{};
        if (!decode_round_input(frame, batch)) {
            return InputDecision::malformed;
        }
        if (batch.round != round_) {
            return InputDecision::wrong_round;
        }

        const Put put = inputs_.put_batch(
            side, batch.inputs.first_tick,
            batch.inputs.masks.data(), batch.inputs.count);
        return map_put(put);
    }

    // Removes and returns the next ready tick for both sides. On failure the
    // outputs are left unchanged; the two references must be distinct.
    bool take(std::uint8_t& host, std::uint8_t& peer) noexcept {
        if (!active_) {
            return false;
        }
        if (&host == &peer) {
            return false;
        }

        std::uint8_t host_value = 0;
        std::uint8_t peer_value = 0;
        if (!inputs_.peek(host_value, peer_value)) {
            return false;
        }
        if (!inputs_.consume()) {
            return false;
        }

        host = host_value;
        peer = peer_value;
        return true;
    }

    // Next tick tracked by the input window.
    std::uint64_t next_tick() const noexcept {
        return inputs_.next_tick();
    }

private:
    static InputDecision map_put(Put put) noexcept {
        switch (put) {
            case Put::stored:
                return InputDecision::stored;
            case Put::duplicate:
                return InputDecision::duplicate;
            case Put::conflict:
                return InputDecision::conflict;
            case Put::stale:
                return InputDecision::stale;
            case Put::too_far:
                return InputDecision::too_far;
            case Put::exhausted:
                return InputDecision::exhausted;
            case Put::invalid:
                return InputDecision::malformed;
        }
        return InputDecision::malformed;
    }

    const std::uint64_t round_;
    const std::uint64_t host_;
    const std::uint64_t peer_;
    TickInputs inputs_{};
    bool active_ = false;
    bool ended_ = false;
};

}  // namespace study_net

#endif  // STUDY_NET_INPUT_AUTHORITY_H

