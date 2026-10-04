#pragma once
#include "net/authoritative_match.h"
#include "net/tick_allowance.h"
#include <cstdint>

namespace study_net {
enum class PaceFailure { none, too_fast, total_limit, clock_regression };
struct PacedReply {
    InputDecision input = InputDecision::inactive;
    PaceFailure failure = PaceFailure::none;
};

// Single-owner adapter. Timestamps are trusted monotonic nanoseconds supplied
// by the connection owner, never fields decoded from the client frame.
class PacedMatch {
public:
    PacedMatch(std::uint64_t round, std::uint64_t host, std::uint64_t peer,
               std::uint64_t seed, PacePolicy policy, std::int64_t started_ns)
        : allowance_(policy), match_(round, host, peer, seed, policy.max_ticks),
          round_(round), host_(host), peer_(peer),
          started_ns_(started_ns), last_ns_(started_ns) {}

    PacedReply submit(std::uint64_t actor, const Frame& frame,
                      std::int64_t now_ns) noexcept {
        if (match_.result().state != MatchState::incomplete) return {};
        RoundBatch batch;
        // Let the existing authority classify out-of-scope or malformed input.
        // Such packets cannot modify the pacing clock or use it to abort others.
        if ((actor != host_ && actor != peer_) || frame.type != kRoundInputType ||
            !decode_round_input(frame, batch) || batch.round != round_) {
            return {match_.submit(actor, frame), PaceFailure::none};
        }
        const auto end = std::uint64_t(batch.inputs.first_tick) + batch.inputs.count;
        if (end > allowance_.policy().max_ticks) return fail(PaceFailure::total_limit);
        if (now_ns < started_ns_ || now_ns < last_ns_)
            return fail(PaceFailure::clock_regression);
        // Ordered signed timestamps may span more than INT64_MAX. Conversion
        // followed by unsigned subtraction represents that nonnegative distance.
        const auto elapsed = std::uint64_t(now_ns) - std::uint64_t(started_ns_);
        if (end > allowance_.allowed(elapsed)) return fail(PaceFailure::too_fast);
        const auto decision = match_.submit(actor, frame);
        if (decision == InputDecision::stored || decision == InputDecision::duplicate)
            last_ns_ = now_ns;
        return {decision, PaceFailure::none};
    }
    void abort() noexcept { match_.abort(); }
    const VerifiedMatch& result() const { return match_.result(); }
    const study_combat::Duel& state() const { return match_.state(); }
    std::optional<MatchRecord> record(std::uint64_t key) const noexcept {
        return match_.record(key);
    }
    PaceFailure failure() const noexcept { return failure_; }
private:
    PacedReply fail(PaceFailure reason) noexcept {
        failure_ = reason;
        match_.abort();
        return {InputDecision::inactive, reason};
    }
    TickAllowance allowance_;
    AuthoritativeMatch match_;
    const std::uint64_t round_, host_, peer_;
    const std::int64_t started_ns_;
    std::int64_t last_ns_;
    PaceFailure failure_ = PaceFailure::none;
};
} // namespace study_net
