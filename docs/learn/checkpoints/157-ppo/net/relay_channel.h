#pragma once
#include "net/paced_match.h"
#include "net/result_handoff.h"

namespace study_net {
struct ChannelView {
    VerifiedMatch match;
    ResultHandoff::Stage saving;
};

// All calls belong to one ownership boundary supplied by an adapter.
class RelayChannel {
public:
    RelayChannel(std::uint64_t key, std::uint64_t round, std::uint64_t host,
                 std::uint64_t peer, std::uint64_t seed, PacePolicy policy,
                 std::int64_t started_ns)
        : match_(round,host,peer,seed,policy,started_ns), saving_(key) {}
    PacedReply receive(std::uint64_t actor,const Frame& frame,std::int64_t now_ns) {
        if(saving_.stage()!=ResultHandoff::Stage::open)return {};
        return match_.submit(actor,frame,now_ns);
    }
    // Seals input and captures a request in the same owner operation. A trigger
    // can seal an incomplete match; that case has no record to send.
    bool claim() { return saving_.claim(match_.record(saving_.key())); }
    std::optional<MatchRecord> request() const { return saving_.request(); }
    bool complete(std::uint64_t key,const Reply& reply) { return saving_.complete(key,reply); }
    ChannelView view() const { return {match_.result(),saving_.stage()}; }
private:
    PacedMatch match_;
    ResultHandoff saving_;
};
} // namespace study_net
