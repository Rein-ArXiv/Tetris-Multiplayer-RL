#include "tests/authority_fixture.h"
#include "net/match_input_stream.h"
#include "net/reactor_choice.h"
#include "net/timer_heap.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
#include <memory>
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
using study_authority_test::frame;
using study_authority_test::same_duel;
void offer(Socket& socket,const Frame& f) {
    EncodedFrame encoded;require(encode_frame(f,encoded),"frame encode");
    require(send_bounded(socket,encoded.bytes.data(),encoded.size,TimerHeap::Clock::now()+1s).outcome==SendOutcome::complete,"frame send");
}
int main(int argc,char** argv) {try {
    Runtime runtime;require(runtime.ready(),"runtime");
    const std::uint64_t seed=77;
    AuthoritativeMatch match(study_authority_test::round_id,study_authority_test::host_id,
                             study_authority_test::peer_id,seed,study_authority_test::test_budget);
    CallbackLoop loop(choose_reactor(epoll_argument(argc,argv)));
    std::size_t handled=0,denied=0;
    auto bind=[&](std::uint64_t actor) {
        auto link=pair();auto stream=std::make_shared<MatchInputStream>(match,actor);
        require(bool(loop.attach(std::move(link.receiver),Read,
            [&,stream](CallbackLoop& owner,Socket& socket,const ReadyEvent& event) {
                if(!(event.readable||event.error))return;
                std::uint8_t bytes[3];const auto read=try_receive(socket,bytes,sizeof(bytes));
                if(read.state==ReceiveState::would_block||read.state==ReceiveState::interrupted)return;
                if(read.state!=ReceiveState::progress){owner.close(event.id);return;}
                MatchInputStream::Report report;const bool ok=stream->feed(bytes,read.count,report);
                handled+=report.handled;denied+=report.denied;
                if(!ok){match.abort();owner.close(event.id);}
            })),"attach");
        return std::move(link.sender);
    };
    auto host=bind(study_authority_test::host_id),peer=bind(study_authority_test::peer_id),outsider=bind(99);
    auto until=[&](std::size_t count) {
        const auto end=TimerHeap::Clock::now()+2s;
        while(handled<count && TimerHeap::Clock::now()<end)loop.dispatch(loop.observe(10));
        require(handled==count,"processing deadline");
    };
    auto claim=frame(0,{0});claim.type=42;offer(host,claim);offer(peer,claim);offer(outsider,frame(0,{0}));until(3);
    require(denied==3 && match.result().ticks==0 && !match.record(7),"claims have no authority");
    const auto round=study_round::Round::create_seeded(study_grid::Grid{},seed);require(bool(round),"seeded rules");
    study_combat::Duel expected(*round,*round);
    std::uint32_t tick=0;
    while(match.result().state==MatchState::incomplete) {
        require(tick<study_authority_test::test_budget,"finite experiment");
        const auto count=handled;
        // Only peer input arrives first; the host's neutral input is still required.
        offer(peer,frame(tick,{study_input::drop}));until(count+1);
        require(match.result().ticks==tick,"missing is not neutral");
        offer(host,frame(tick,{0}));until(count+2);
        require(bool(expected.tick(*study_input::decode(0),*study_input::decode(study_input::drop))),"direct rule step");
        ++tick;same_duel(match.state(),expected);
    }
    require(match.result().state==MatchState::finished && match.result().winner==Winner::host,"terminal winner");
    const auto record=match.record(7);require(record.has_value(),"server record");
    MatchSubmission submission(2);require(submission.prepare(*record),"submission prepare");
    unsigned calls=0;
    const auto sender=[&](const MatchRecord& r)noexcept {++calls;return Reply{Reply::confirmed,{r.key,1,r.player_a,r.player_b}};};
    require(submission.submit(sender)==MatchSubmission::SubmitStep::confirmed && calls==1,"trusted port");
    std::printf("actual TCP: claims rejected, missing side waits, %u paired rule steps, terminal server record; storage port is a test double\n",tick);
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
