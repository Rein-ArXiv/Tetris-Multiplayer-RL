#pragma once
#include "net/relay_channel.h"
#include "net/monotonic_id.h"
#include "net/offload.h"
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace study_net {
// Single event-loop owner, with blocking storage delegated to Offload workers.
// Local IDs never repeat within this registry. Durable result keys are separate
// server identities and must remain unique across registry/process restarts.
class LoopRelayChannels {
public:
    using Id=std::uint32_t;
    using Sender=std::function<Reply(const MatchRecord&)>;
    LoopRelayChannels(std::size_t workers,std::size_t capacity,std::function<void()> wake)
        : owner_(std::this_thread::get_id()),pool_(workers,std::move(wake),capacity) {}
    ~LoopRelayChannels() { shutdown(); }
    template<class... Args> Id open(Args&&... args) {
        require_owner();
        if(stopping_)return 0;
        const auto id=ids_.take();if(!id)return 0;
        channels_.emplace(id,std::make_unique<RelayChannel>(std::forward<Args>(args)...));
        return id;
    }
    template<class Clock>
    PacedReply receive(Id id,std::uint64_t actor,const Frame& frame,Clock&& clock) {
        require_owner();
        const auto it=channels_.find(id);
        if(stopping_ || it==channels_.end())return {};
        return it->second->receive(actor,frame,clock());
    }
    bool finalize(Id id,Sender sender) {
        require_owner();
        const auto it=channels_.find(id);
        if(stopping_ || it==channels_.end() || !it->second->claim())return false;
        const auto request=it->second->request();
        if(!request)return true;
        const auto key=request->key;
        try {
            const bool queued=pool_.submit(
                [this,id,request=*request,sender=std::move(sender)]() -> Offload::Cont {
                    const auto reply=sender(request);
                    // Worker copies values; only this continuation reads the registry.
                    return [this,id,key=request.key,reply] { complete(id,key,reply); };
                },[this,id,key] { complete(id,key,Reply{}); });
            if(!queued)complete(id,key,Reply{});
        } catch(...) { complete(id,key,Reply{}); }
        return true;
    }
    std::optional<ChannelView> view(Id id) const {
        require_owner();const auto it=channels_.find(id);
        return it==channels_.end()?std::nullopt:std::optional<ChannelView>(it->second->view());
    }
    bool erase(Id id) { require_owner();return channels_.erase(id)!=0; }
    void drain() {
        require_owner();std::vector<Offload::Cont> ready;
        pool_.drain(ready);for(auto& continuation:ready)if(continuation)continuation();
    }
    void shutdown() {
        require_owner();
        if(stopping_)return;
        stopping_=true;
        pool_.shutdown(); // Accepted work finishes before this registry is destroyed.
        drain();
    }
private:
    void require_owner() const {
        if(std::this_thread::get_id()!=owner_)throw std::logic_error("loop owner required");
    }
    void complete(Id id,std::uint64_t key,const Reply& reply) {
        require_owner();const auto it=channels_.find(id);
        if(it!=channels_.end())it->second->complete(key,reply);
    }
    const std::thread::id owner_;
    MonotonicId ids_;
    std::map<Id,std::unique_ptr<RelayChannel>> channels_;
    bool stopping_=false;
    Offload pool_;
};
} // namespace study_net
