#pragma once
#include "net/relay_channel.h"
#include <mutex>
#include <utility>

namespace study_net {
// Callers keep this object alive until every receiving/finalizing thread joins.
// The adapter owns state synchronization; it does not own sockets or threads.
class ThreadRelayChannel {
public:
    template<class... Args>
    explicit ThreadRelayChannel(Args&&... args):channel_(std::forward<Args>(args)...) {}
    template<class Clock>
    PacedReply receive(std::uint64_t actor,const Frame& frame,Clock&& clock) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Sample the trusted clock AFTER serialization, not in waiting readers.
        return channel_.receive(actor,frame,clock());
    }
    template<class Sender>
    bool finalize(Sender&& sender) {
        std::optional<MatchRecord> request;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(!channel_.claim())return false;
            request=channel_.request();
        }
        if(!request)return true; // Incomplete/invalid/budget-exhausted: no storage.
        Reply reply;
        try { reply=sender(*request); }
        catch(...) { reply=Reply{}; } // Failure to confirm is not rollback proof.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel_.complete(request->key,reply);
        }
        return true;
    }
    ChannelView view() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return channel_.view();
    }
private:
    mutable std::mutex mutex_;
    RelayChannel channel_;
};
} // namespace study_net
