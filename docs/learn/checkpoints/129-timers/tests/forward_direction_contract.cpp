#include "net/forward_direction.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>
using namespace study_net;
void require(bool ok) { if (!ok) throw std::runtime_error("forward direction contract"); }
std::vector<std::uint8_t> wire(std::uint8_t type, std::size_t count) {
    Frame f; f.type=type; f.size=count;
    for (std::size_t i=0;i<count;++i) f.payload[i]=static_cast<std::uint8_t>(i);
    EncodedFrame e; require(encode_frame(f,e));return {e.bytes.begin(),e.bytes.begin()+e.size};
}
int main() {
    static_assert(!std::is_copy_constructible_v<ForwardDirection>);
    static_assert(!std::is_move_constructible_v<ForwardDirection>);
    std::size_t cases=0;
    // Independent expected wire bytes; any receive split, any short-send cap.
    for (std::size_t size=0;size<=kMaxPayloadBytes;++size) {
        const auto a=wire(41,size),b=wire(42,3);
        auto expected=a;expected.insert(expected.end(),b.begin(),b.end());
        for (std::size_t split=0;split<=expected.size();++split) {
            for (std::size_t cap=1;cap<=7;++cap) {
                ForwardDirection direction;
                std::vector<std::uint8_t> actual;actual.reserve(expected.size());
                unsigned calls=0;
                auto sender=[&](const std::uint8_t* data,std::size_t remaining) noexcept {
                    ++calls;
                    if (calls%5==1) return SendAttempt{SendState::would_block,0,0};
                    if (calls%5==2) return SendAttempt{SendState::interrupted,0,0};
                    const auto n=(std::min)(cap,remaining);
                    actual.insert(actual.end(),data,data+n);return SendAttempt{SendState::progress,n,0};
                };
                require(direction.feed(expected.data(),split));
                while(direction.has_pending()) { auto before=calls;direction.flush(sender);require(calls==before+1); }
                require(direction.can_read());
                require(direction.feed(expected.data()+split,expected.size()-split));
                while(direction.has_pending()) direction.flush(sender);
                require(actual==expected && direction.can_read() && direction.buffered_bytes()==0);
                require(direction.eof()==ForwardState::source_closed);
                require(direction.stop()==ForwardState::source_closed);++cases;
            }
        }
    }
    {
        const auto bytes=wire(41,3);FrameParser prefix;
        require(prefix.append(bytes.data(),bytes.size()));
        ForwardDirection d(std::move(prefix));require(prefix.pending_bytes()==0 && !d.can_read());
        const auto pending=d.bytes_pending();require(!d.feed(bytes.data(),bytes.size()) && d.bytes_pending()==pending);
        d.flush([](const std::uint8_t*,std::size_t) noexcept {return SendAttempt{SendState::progress,2,0};});
        require(d.bytes_pending()==pending-2);
        d.flush([](const std::uint8_t*,std::size_t) noexcept {return SendAttempt{SendState::error,0,77};});
        require(d.state()==ForwardState::send_error && d.last_error()==77 && d.bytes_pending()==pending-2);
        unsigned calls=0;d.flush([&](const std::uint8_t*,std::size_t) noexcept {++calls;return SendAttempt{SendState::error,0,1};});
        require(calls==0);
    }
    const SendAttempt invalid[]={{SendState::progress,0,0},{SendState::progress,100,0},
        {SendState::progress,1,1},{SendState::would_block,1,0},{SendState::interrupted,0,2},
        {SendState::error,1,7},{static_cast<SendState>(99),0,0}};
    for (auto result:invalid) {
        ForwardDirection d;const auto bytes=wire(41,1);require(d.feed(bytes.data(),bytes.size()));
        const auto count=d.bytes_pending();d.flush([=](const std::uint8_t*,std::size_t) noexcept{return result;});
        require(d.state()==ForwardState::invalid_send_result && d.bytes_pending()==count);
    }
    {
        auto bytes=wire(41,1);const auto good=bytes.size();bytes.push_back(0);bytes.push_back(0);
        ForwardDirection d;require(d.feed(bytes.data(),bytes.size()));require(d.state()==ForwardState::running);
        std::size_t sent=0;
        d.flush([&](const std::uint8_t*,std::size_t size) noexcept {sent+=size;return SendAttempt{SendState::progress,size,0};});
        require(sent==good && d.state()==ForwardState::protocol_error && !d.can_read());
    }
    {
        ForwardDirection d;const std::uint8_t first=2;require(d.feed(&first,1));
        require(d.eof()==ForwardState::truncated);
        ForwardDirection full;const auto frame=wire(1,0);full.feed(frame.data(),frame.size());
        require(full.eof()==ForwardState::undelivered);
        ForwardDirection limited;std::uint8_t many[kReceiveCapacity+1]{};
        require(!limited.feed(many,sizeof(many)) && limited.state()==ForwardState::buffer_limit);
        ForwardDirection stopped;require(stopped.stop()==ForwardState::cancelled && !stopped.can_read());
        require(!stopped.feed(nullptr,0));
    }
    std::printf("forward direction: %zu receive-split/send-cap cases; prefix/backpressure/error/EOF/terminal contracts\n",cases);
}
