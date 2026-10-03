#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

#include "net/input_authority.h"
#include "net/match_submission.h"
#include "simulation/duel.h"
#include "simulation/input_mask.h"

// Headless, deterministic authoritative match driver.
//
// Ownership: every member is owned by a single thread. If more than one thread
// touches an AuthoritativeMatch, the caller must provide external
// synchronisation (mutex / strand). There are no sockets, no HTTP, no database
// writes and no wall-clock pacing yet: progress requires paired inputs and is bounded by the
// constructor's finite tick budget.
//
// Trust model: the server seed and the empty starting board are created
// internally from trusted constructor arguments. Untrusted clients cannot
// inject their own board or seed; their only influence is the input frames
// routed through InputAuthority.
namespace study_net {

enum class MatchState {
    incomplete,
    finished,
    invalid,
    budget_exhausted,
};

enum class Winner {
    none,
    host,
    peer,
    draw,
};

struct VerifiedMatch {
    MatchState state = MatchState::incomplete;
    Winner winner = Winner::none;
    std::uint64_t ticks = 0;
    std::uint64_t score_host = 0;
    std::uint64_t score_peer = 0;
    std::uint64_t lines_host = 0;
    std::uint64_t lines_peer = 0;
};

class AuthoritativeMatch {
public:
    AuthoritativeMatch(std::uint64_t round,
                       std::uint64_t host,
                       std::uint64_t peer,
                       std::uint64_t server_seed,
                       std::uint64_t max_ticks)
        : authority_(round, host, peer),
          duel_(make_duel(server_seed)),
          max_ticks_(validate_budget(max_ticks)),
          round_(round), host_(host), peer_(peer) {
        if (!authority_.start()) {
            throw std::invalid_argument(
                "AuthoritativeMatch: input authority failed to start");
        }
    }

    AuthoritativeMatch(const AuthoritativeMatch&) = delete;
    AuthoritativeMatch& operator=(const AuthoritativeMatch&) = delete;
    AuthoritativeMatch(AuthoritativeMatch&&) = delete;
    AuthoritativeMatch& operator=(AuthoritativeMatch&&) = delete;

    InputDecision submit(std::uint64_t actor, const Frame& frame) noexcept {
        if (is_terminal(result_.state)) {
            return InputDecision::inactive;
        }

        const InputDecision decision = authority_.submit(actor, frame);

        if (decision == InputDecision::malformed ||
            decision == InputDecision::conflict) {
            invalidate();
            return decision;
        }

        if (decision == InputDecision::stored ||
            decision == InputDecision::duplicate) {
            drain();
        }

        return decision;
    }

    // Called by the connection owner for an invalid frame stream.
    void abort() noexcept {
        if (result_.state == MatchState::incomplete) invalidate();
    }

    const VerifiedMatch& result() const { return result_; }

    // Only a terminal simulation may cross the trusted result-submission port.
    // The external service still decides whether persistence/reward succeeds.
    std::optional<MatchRecord> record(std::uint64_t key) const noexcept {
        if (result_.state != MatchState::finished || key == 0 ||
            result_.lines_host > std::numeric_limits<std::uint32_t>::max() ||
            result_.lines_peer > std::numeric_limits<std::uint32_t>::max()) return {};
        MatchRecord out;
        out.key = key; out.round = round_; out.player_a = host_; out.player_b = peer_;
        out.ticks = result_.ticks;
        out.score_a = result_.score_host; out.score_b = result_.score_peer;
        out.lines_a = static_cast<std::uint32_t>(result_.lines_host);
        out.lines_b = static_cast<std::uint32_t>(result_.lines_peer);
        out.winner = result_.winner == Winner::host ? MatchRecord::a
                   : result_.winner == Winner::peer ? MatchRecord::b : MatchRecord::draw;
        return out;
    }

    const study_combat::Duel& state() const { return duel_; }

private:
    static bool is_terminal(MatchState state) {
        return state == MatchState::finished ||
               state == MatchState::invalid ||
               state == MatchState::budget_exhausted;
    }

    static std::uint64_t validate_budget(std::uint64_t max_ticks) {
        const std::uint64_t limit =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::uint32_t>::max()) + 1u;
        if (max_ticks == 0 || max_ticks > limit) {
            throw std::invalid_argument(
                "AuthoritativeMatch: max_ticks must be in [1, UINT32_MAX + 1]");
        }
        return max_ticks;
    }

    static study_combat::Duel make_duel(std::uint64_t seed) {
        const std::optional<study_round::Round> seeded =
            study_round::Round::create_seeded(study_grid::Grid{}, seed);
        if (!seeded.has_value()) {
            throw std::invalid_argument(
                "AuthoritativeMatch: unable to create seeded round");
        }
        return study_combat::Duel(*seeded, *seeded);
    }

    void drain() noexcept {
        while (result_.state == MatchState::incomplete) {
            std::uint8_t host_mask = 0;
            std::uint8_t peer_mask = 0;
            if (!authority_.take(host_mask, peer_mask)) {
                return;
            }

            const auto host_intent = study_input::decode(host_mask);
            const auto peer_intent = study_input::decode(peer_mask);
            if (!host_intent.has_value() || !peer_intent.has_value()) {
                invalidate();
                return;
            }

            const auto step = duel_.tick(*host_intent, *peer_intent);
            if (!step.has_value()) {
                invalidate();
                return;
            }

            result_.ticks += 1;

            const bool host_finished = duel_.left().finished();
            const bool peer_finished = duel_.right().finished();
            if (host_finished || peer_finished) {
                finish(host_finished, peer_finished);
                return;
            }

            if (result_.ticks >= max_ticks_) {
                result_.state = MatchState::budget_exhausted;
                result_.winner = Winner::none;
                close_gate();
                return;
            }
        }
    }

    void finish(bool host_finished, bool peer_finished) noexcept {
        result_.state = MatchState::finished;
        if (host_finished && peer_finished) {
            result_.winner = Winner::draw;
        } else if (peer_finished) {
            result_.winner = Winner::host;
        } else {
            result_.winner = Winner::peer;
        }
        result_.score_host = duel_.left().score();
        result_.score_peer = duel_.right().score();
        result_.lines_host = duel_.left().total_lines();
        result_.lines_peer = duel_.right().total_lines();
        close_gate();
    }

    void invalidate() noexcept {
        result_.state = MatchState::invalid;
        result_.winner = Winner::none;
        result_.score_host = 0;
        result_.score_peer = 0;
        result_.lines_host = 0;
        result_.lines_peer = 0;
        close_gate();
    }

    void close_gate() noexcept {
        if (!closed_) {
            authority_.close();
            closed_ = true;
        }
    }

    InputAuthority authority_;
    study_combat::Duel duel_;
    const std::uint64_t max_ticks_;
    const std::uint64_t round_, host_, peer_;
    VerifiedMatch result_;
    bool closed_ = false;
};

}  // namespace study_net
