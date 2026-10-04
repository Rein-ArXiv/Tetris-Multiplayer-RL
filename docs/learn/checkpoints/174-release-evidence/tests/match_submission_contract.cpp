#include "net/match_submission.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
using namespace study_net;
using State = MatchSubmission::SubmissionState;
using Step = MatchSubmission::SubmitStep;
void check(bool ok) { if (!ok) throw std::runtime_error("match submission contract"); }
int main() {
    try {
        MatchRecord original{17,3,101,202,60,100,20,4,1,MatchRecord::a};
        unsigned calls = 0;
        auto sender = [&](const MatchRecord& r) noexcept {
            ++calls; return Reply{Reply::confirmed,{r.key,9,r.player_a,r.player_b}};
        };
        MatchSubmission empty(3);
        check(empty.submit(sender)==Step::no_work && calls==0 && !empty.request() && !empty.receipt());
        for (int field=0;field<7;++field) {
            auto bad=original;
            switch(field) {
                case 0:bad.key=0;break; case 1:bad.round=0;break;
                case 2:bad.player_a=0;break;case 3:bad.player_b=0;break;
                case 4:bad.player_b=bad.player_a;break;case 5:bad.ticks=0;break;
                case 6:bad.winner=static_cast<MatchRecord::MatchWinner>(255);break;
            }
            check(!empty.prepare(bad) && empty.state()==State::empty && !empty.request());
        }
        MatchSubmission zero(0);check(zero.prepare(original));
        check(zero.submit(sender)==Step::budget_exhausted && calls==0 && zero.state()==State::ready);
        MatchSubmission pending(2);auto mutable_record=original;
        check(pending.prepare(mutable_record));mutable_record.score_a=999;
        check(*pending.request()==original && !pending.prepare(mutable_record));
        const auto* owned=pending.request();
        auto lost = [&](const MatchRecord& r) noexcept {
            ++calls;
            if (&r!=owned || r!=original || pending.state()!=State::unconfirmed) std::terminate();
            return Reply{};
        };
        check(pending.submit(lost)==Step::unconfirmed && !pending.receipt());
        check(pending.submit(lost)==Step::unconfirmed);
        check(pending.submit(sender)==Step::budget_exhausted && calls==2 && pending.attempts()==2);
        check(pending.state()==State::unconfirmed && *pending.request()==original);
        for (int field=0;field<5;++field) {
            MatchSubmission trial(2);check(trial.prepare(original));
            Reply wrong{Reply::confirmed,{original.key,9,original.player_a,original.player_b}};
            switch(field){case 0:++wrong.receipt.key;break;case 1:wrong.receipt.row=0;break;
                case 2:++wrong.receipt.player_a;break;case 3:++wrong.receipt.player_b;break;
                case 4:wrong.kind=static_cast<Reply::ReplyKind>(255);break;}
            check(trial.submit([&](const MatchRecord&) noexcept {return wrong;})==Step::stopped);
            check(!trial.receipt());const auto before=calls;
            check(trial.submit(sender)==Step::no_work && calls==before);
            check(trial.state()==State::stopped && *trial.request()==original);
            check(!trial.prepare(original));
        }
        // Equality is part of the service's same-operation check, not just key equality.
        for(int field=0;field<11;++field){
            auto changed=original;
            switch(field){case 0:++changed.key;break;case 1:++changed.round;break;
                case 2:++changed.player_a;break;case 3:++changed.player_b;break;
                case 4:++changed.ticks;break;case 5:++changed.score_a;break;
                case 6:++changed.score_b;break;case 7:++changed.lines_a;break;
                case 8:++changed.lines_b;break;case 9:changed.winner=MatchRecord::b;break;
                default:changed.ticks=std::numeric_limits<std::uint64_t>::max();break;}
            check(changed!=original);
        }
        std::puts("submission: immutable request, validation, receipts, attempt limits and uncertain outcome agree");
    } catch (const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
