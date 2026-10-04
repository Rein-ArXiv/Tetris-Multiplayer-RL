#include "net/result_notice.h"
#include "net/final_output.h"
#include "tests/authority_fixture.h"
#include <cstring>
#include <iostream>
using namespace study_net;
using study_authority_test::require;
using Stage=ResultHandoff::Stage;
using namespace std::chrono_literals;
int main(){try{
    ChannelView view{};view.saving=Stage::open;require(notice_reason(view)==NoticeReason::waiting);
    for(auto stage:{Stage::confirmed,Stage::inflight,Stage::unconfirmed}){
        view.saving=stage;view.match.state=MatchState::incomplete;require(notice_reason(view)==NoticeReason::unknown);
    }
    view.saving=Stage::not_eligible;
    for(auto pair:{std::pair{MatchState::incomplete,NoticeReason::incomplete},
                   std::pair{MatchState::invalid,NoticeReason::invalid},
                   std::pair{MatchState::budget_exhausted,NoticeReason::budget_exhausted}}){
        view.match.state=pair.first;require(notice_reason(view)==pair.second);
    }
    view.match.state=MatchState::finished;view.match.winner=Winner::host;
    view.saving=Stage::inflight;require(notice_reason(view)==NoticeReason::pending);
    view.saving=Stage::confirmed;require(notice_reason(view)==NoticeReason::saved);
    view.match.winner=Winner::peer;require(notice_reason(view)==NoticeReason::saved);
    view.match.winner=Winner::draw;require(notice_reason(view)==NoticeReason::draw);
    view.match.winner=Winner::none;require(notice_reason(view)==NoticeReason::unknown);
    view.saving=Stage::unconfirmed;require(notice_reason(view)==NoticeReason::unconfirmed);
    Frame f;require(encode_notice({7,NoticeReason::saved},f));
    NoticeView ui(7);require(ui.reason()==NoticeReason::waiting && ui.accept(f) && ui.reason()==NoticeReason::saved);
    auto other=f;other.payload[0]=8;require(!ui.accept(other) && ui.reason()==NoticeReason::saved);
    for(unsigned raw=0;raw<256;++raw){auto next=f;next.payload[8]=raw;ResultNotice decoded;
        require(decode_notice(next,decoded) && decoded.key==7);
        require(decoded.reason==(raw<=8?static_cast<NoticeReason>(raw):NoticeReason::unknown));
        require(std::strlen(notice_text(decoded.reason))>0);
    }
    for(auto size:{std::size_t(0),std::size_t(8),std::size_t(10),std::size_t(99)}){
        auto bad=f;bad.size=size;require(!ui.accept(bad) && ui.reason()==NoticeReason::saved);
    }
    auto unknown=f;unknown.payload[8]=255;require(ui.accept(unknown) && ui.reason()==NoticeReason::unknown);
    // A pending prefix and a result share ONE byte queue. Would-block keeps the suffix.
    std::atomic<std::size_t> total{0};FinalOutput<128,96,32> output(total,128);
    Frame previous;previous.type=8;previous.size=2;previous.payload[0]=5;previous.payload[1]=6;
    EncodedFrame prefix,result;require(encode_frame(previous,prefix) && encode_frame(f,result));
    require(output.enqueue(previous)==BufferResult::stored);
    std::vector<std::uint8_t> wire;wire.reserve(128);
    auto short_send=[&](const std::uint8_t* data,std::size_t count)noexcept{
        const auto n=std::min(std::size_t(2),count);wire.insert(wire.end(),data,data+n);return SendAttempt{SendState::progress,n,0};};
    const FinalOutput<128,96,32>::Clock::time_point now{};
    require(output.writable(now,short_send)==DeliveryState::open);
    require(output.enqueue(f)==BufferResult::stored);output.begin_close(now+1s);
    require(!output.accepts_input() && output.enqueue(f)==BufferResult::closed);
    auto blocked=[](const std::uint8_t*,std::size_t)noexcept{return SendAttempt{SendState::would_block,0,0};};
    const auto held=output.pending();require(output.writable(now,blocked)==DeliveryState::draining && output.pending()==held);
    while(output.wants_write())output.writable(now,short_send);
    std::vector<std::uint8_t> expected(prefix.bytes.begin(),prefix.bytes.begin()+prefix.size);
    expected.insert(expected.end(),result.bytes.begin(),result.bytes.begin()+result.size);
    require(wire==expected && total==0 && output.state()==DeliveryState::drained);
    {FinalOutput<64,48,16> stuck(total,64);require(stuck.enqueue(f)==BufferResult::stored);
     stuck.begin_close(now+1s);stuck.begin_close(now+10s);stuck.expire(now+1s);
     require(stuck.state()==DeliveryState::timed_out && total==0 && !stuck.wants_write());}
    {FinalOutput<64,48,16> failed(total,64);require(failed.enqueue(f)==BufferResult::stored);
     const auto error=[](const std::uint8_t*,std::size_t)noexcept{return SendAttempt{SendState::error,0,5};};
     require(failed.writable(now,error)==DeliveryState::failed && total==0);}
    {FinalOutput<64,48,16> limited(total,1);require(limited.enqueue(f)==BufferResult::global_limit && total==0);
     limited.begin_close(now);require(limited.state()==DeliveryState::drained);}
    {FinalOutput<64,48,16> abandoned(total,64);require(abandoned.enqueue(f)==BufferResult::stored);}
    require(total==0);
    std::cout<<"notice state, unknown compatibility, fixed key, FIFO, partial progress, deadline and budget release passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
