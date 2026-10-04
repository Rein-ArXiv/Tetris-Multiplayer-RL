#pragma once
#include "net/pending_send.h"
#include "net/framing.h"
#include <chrono>
#include <utility>
namespace study_net {
enum class DeliveryState { open,draining,drained,timed_out,failed };
// A single connection owner appends ALL outbound frames through this FIFO.
// No socket close occurs here: the owner observes terminal state and closes it.
template<std::size_t Capacity,std::size_t High,std::size_t Low>
class FinalOutput {
public:
    using Clock=std::chrono::steady_clock;
    FinalOutput(std::atomic<std::size_t>& total,std::size_t limit):queue_(total,limit) {}
    BufferResult enqueue(const Frame& frame) noexcept {
        if(state_!=DeliveryState::open)return BufferResult::closed;
        EncodedFrame bytes;
        if(!encode_frame(frame,bytes))return BufferResult::invalid;
        return queue_.append(bytes.bytes.data(),bytes.size);
    }
    void begin_close(Clock::time_point deadline) noexcept {
        if(state_!=DeliveryState::open)return; // Never extend an existing deadline.
        deadline_=deadline;
        state_=DeliveryState::draining;
        if(queue_.size()==0)finish(DeliveryState::drained);
    }
    template<class Sender> DeliveryState writable(Clock::time_point now,Sender&& sender) noexcept {
        if(state_!=DeliveryState::open && state_!=DeliveryState::draining)return state_;
        if(state_==DeliveryState::draining && now>=deadline_) {
            finish(DeliveryState::timed_out);return state_;
        }
        const auto result=queue_.flush(std::forward<Sender>(sender));
        if(result==FlushResult::error || result==FlushResult::invalid)finish(DeliveryState::failed);
        else if(state_==DeliveryState::draining && queue_.size()==0)finish(DeliveryState::drained);
        return state_;
    }
    // A timer calls this even when no writable event arrives.
    void expire(Clock::time_point now) noexcept {
        if(state_==DeliveryState::draining && now>=deadline_)finish(DeliveryState::timed_out);
    }
    void abort() noexcept {if(state_==DeliveryState::open || state_==DeliveryState::draining)finish(DeliveryState::failed);}
    DeliveryState state()const noexcept{return state_;}
    bool wants_write()const noexcept{return queue_.size()!=0;}
    bool accepts_input()const noexcept{return state_==DeliveryState::open;}
    std::size_t pending()const noexcept{return queue_.size();}
private:
    void finish(DeliveryState state) noexcept {queue_.close();state_=state;}
    PendingSend<Capacity,High,Low> queue_;
    DeliveryState state_=DeliveryState::open;
    Clock::time_point deadline_{};
};
} // namespace study_net
