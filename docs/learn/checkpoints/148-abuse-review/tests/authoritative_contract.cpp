#include "tests/authority_fixture.h"
#include <array>
#include <utility>
#include <iostream>
using namespace study_net;
using namespace study_authority_test;
struct Trace {
    std::vector<std::uint8_t> host,peer;
    study_combat::Duel duel;
};
Trace trace(std::uint64_t seed,std::uint8_t host,std::uint8_t peer) {
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},seed);
    require(bool(round));Trace out{{},{},study_combat::Duel(*round,*round)};
    while (!out.duel.left().finished() && !out.duel.right().finished()) {
        require(out.host.size()<test_budget);
        require(bool(out.duel.tick(*study_input::decode(host),*study_input::decode(peer))));
        out.host.push_back(host);out.peer.push_back(peer);
    }
    return out;
}
void deliver(AuthoritativeMatch& match,const Trace& data,std::size_t group,bool reverse) {
    for(std::size_t offset=0;offset<data.host.size();offset+=group) {
        const auto count=std::min(group,data.host.size()-offset);
        const auto a=frame(static_cast<std::uint32_t>(offset),{data.host.begin()+offset,data.host.begin()+offset+count});
        const auto b=frame(static_cast<std::uint32_t>(offset),{data.peer.begin()+offset,data.peer.begin()+offset+count});
        require(match.submit(reverse?peer_id:host_id,reverse?b:a)==InputDecision::stored);
        require(match.submit(reverse?host_id:peer_id,reverse?a:b)==InputDecision::stored);
    }
}
int main() {try {
    for(std::uint64_t seed:std::array<std::uint64_t,5>{0,1,77,12345,UINT64_MAX}) {
        for(auto masks:{std::pair<std::uint8_t,std::uint8_t>{0,study_input::drop},
                        {study_input::drop,0},{study_input::drop,study_input::drop}}) {
            const auto data=trace(seed,masks.first,masks.second);
            for(std::size_t group:{std::size_t(1),std::size_t(3),kMaxBatchInputs}) for(bool reverse:{false,true}) {
                AuthoritativeMatch match(round_id,host_id,peer_id,seed,test_budget);
                require(!match.record(7));deliver(match,data,group,reverse);
                const auto result=match.result();require(result.state==MatchState::finished && result.ticks==data.host.size());
                same_duel(match.state(),data.duel);
                const auto expected=data.duel.left().finished()==data.duel.right().finished()?Winner::draw:
                    data.duel.left().finished()?Winner::peer:Winner::host;
                require(result.winner==expected && result.score_host==data.duel.left().score() &&
                        result.score_peer==data.duel.right().score() && result.lines_host==data.duel.left().total_lines() &&
                        result.lines_peer==data.duel.right().total_lines());
                auto forged=frame(0,{0});forged.type=255;
                require(match.submit(host_id,forged)==InputDecision::inactive);
                same_duel(match.state(),data.duel);require(match.result().ticks==result.ticks);
                const auto record=match.record(7);require(record && record->player_a==host_id && record->player_b==peer_id);
                MatchSubmission submission(2);require(submission.prepare(*record));
                unsigned calls=0;
                auto sender=[&](const MatchRecord& saved)noexcept {++calls;return Reply{Reply::confirmed,{saved.key,1,saved.player_a,saved.player_b}};};
                require(submission.submit(sender)==MatchSubmission::SubmitStep::confirmed && calls==1);
                require(submission.submit(sender)==MatchSubmission::SubmitStep::no_work && calls==1);
            }
            AuthoritativeMatch exact(round_id,host_id,peer_id,seed,data.host.size());
            deliver(exact,data,1,false);require(exact.result().state==MatchState::finished);
        }
    }
    // The last batch may contain inputs after the first terminal tick.
    // Those suffix inputs must not alter the terminal snapshot.
    const auto terminal_trace=trace(77,0,study_input::drop);
    AuthoritativeMatch suffix(round_id,host_id,peer_id,77,test_budget);
    const auto last=terminal_trace.host.size()-1;
    for(std::size_t tick=0;tick<last;++tick) {
        suffix.submit(host_id,frame(static_cast<std::uint32_t>(tick),{0}));
        suffix.submit(peer_id,frame(static_cast<std::uint32_t>(tick),{study_input::drop}));
    }
    suffix.submit(host_id,frame(static_cast<std::uint32_t>(last),{0,study_input::drop}));
    suffix.submit(peer_id,frame(static_cast<std::uint32_t>(last),{study_input::drop,0}));
    require(suffix.result().state==MatchState::finished && suffix.result().ticks==terminal_trace.host.size());
    same_duel(suffix.state(),terminal_trace.duel);
    AuthoritativeMatch malformed(round_id,host_id,peer_id,77,test_budget);
    auto broken=frame(0,{0});broken.size=1;
    require(malformed.submit(host_id,broken)==InputDecision::malformed);
    require(malformed.result().state==MatchState::invalid && !malformed.record(7));
    require(malformed.submit(peer_id,frame(0,{0}))==InputDecision::inactive);
    AuthoritativeMatch missing(round_id,host_id,peer_id,77,test_budget);
    const auto initial=missing.state();auto neutral=frame(0,{0});
    require(missing.submit(0,neutral)==InputDecision::unauthenticated);
    require(missing.submit(99,neutral)==InputDecision::not_participant);
    auto forged=neutral;forged.type=42;
    require(missing.submit(host_id,forged)==InputDecision::forbidden_type && !missing.record(7));
    require(missing.submit(host_id,neutral)==InputDecision::stored);
    require(missing.submit(host_id,neutral)==InputDecision::duplicate);
    require(missing.result().ticks==0);same_duel(missing.state(),initial);
    require(missing.submit(peer_id,frame(1,{0}))==InputDecision::stored && missing.result().ticks==0);
    require(missing.submit(peer_id,neutral)==InputDecision::stored && missing.result().ticks==1);
    require(missing.submit(host_id,frame(1,{0}))==InputDecision::stored && missing.result().ticks==2);
    require(missing.result().state==MatchState::incomplete && !missing.record(7));
    AuthoritativeMatch invalid(round_id,host_id,peer_id,77,test_budget);
    require(invalid.submit(host_id,neutral)==InputDecision::stored);
    require(invalid.submit(host_id,frame(0,{study_input::drop}))==InputDecision::conflict);
    require(invalid.result().state==MatchState::invalid && !invalid.record(7));same_duel(invalid.state(),initial);
    AuthoritativeMatch bounded(round_id,host_id,peer_id,77,1);
    bounded.submit(host_id,neutral);bounded.submit(peer_id,neutral);
    require(bounded.result().state==MatchState::budget_exhausted && !bounded.record(7));
    require(bounded.result().winner==Winner::none && bounded.result().score_host==0);
    // IDs use the same full-width contract as admission and the round envelope.
    AuthoritativeMatch wide(UINT64_MAX,UINT64_MAX-1,UINT64_MAX-2,77,1);
    RoundBatch batch;batch.round=UINT64_MAX;batch.inputs.count=1;batch.inputs.masks[0]=0;Frame f;
    require(encode_round_input(batch,f));require(wide.submit(UINT64_MAX-1,f)==InputDecision::stored);
    require(wide.submit(UINT64_MAX-2,f)==InputDecision::stored && wide.result().ticks==1);
    bool rejected=false;try{AuthoritativeMatch bad(round_id,host_id,peer_id,77,0);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    std::cout<<"direct rule replay, grouping/order, missing/neutral, terminal/draw, bounded work, full-width IDs and submission gate passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
