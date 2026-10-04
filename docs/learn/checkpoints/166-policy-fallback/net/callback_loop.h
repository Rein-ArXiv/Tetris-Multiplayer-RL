#ifndef STUDY_NET_CALLBACK_LOOP_H
#define STUDY_NET_CALLBACK_LOOP_H
#include "net/study_reactor.h"
#include "net/send_socket.h"
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
namespace study_net {
class CallbackLoop {
 public:
    using Handler = std::function<void(CallbackLoop&, Socket&, const ReadyEvent&)>;
    explicit CallbackLoop(std::unique_ptr<StudyReactor> reactor) : reactor_(std::move(reactor)) {
        if (!reactor_) throw std::invalid_argument("reactor required");
        nodes_.reserve(Registrations::capacity);
    }
    ~CallbackLoop() {
        for (const auto& node : nodes_) reactor_->unwatch(node->id);
        nodes_.clear();
    }
    CallbackLoop(const CallbackLoop&) = delete;
    CallbackLoop& operator=(const CallbackLoop&) = delete;
    std::optional<Registration> attach(Socket socket, unsigned interest, Handler handler) {
        if (!handler || nodes_.size() == Registrations::capacity) return std::nullopt;
        int error = 0;
        if (!set_nonblocking(socket, true, error)) return std::nullopt;
        // Allocate before registering the borrowed fd; reserved vector won't allocate afterwards.
        auto node = std::make_shared<Node>(std::move(socket), std::move(handler));
        const auto id = reactor_->watch(node->socket, interest);
        if (!id) return std::nullopt;
        node->id = *id;
        nodes_.push_back(std::move(node));
        return id;
    }
    bool close(Registration id) {
        auto it = std::find_if(nodes_.begin(), nodes_.end(), [=](const auto& n) { return n->id == id; });
        if (it == nodes_.end() || !reactor_->unwatch(id)) return false;
        // Current handler may still own Node; closure and Socket survive until it returns.
        nodes_.erase(it);
        return true;
    }
    bool change(Registration id, unsigned interest) { return reactor_->change(id, interest); }
    bool current(Registration id) const { return reactor_->current(id); }
    bool wake() noexcept { return reactor_->wake(); }
    ReadyBatch observe(int timeout_ms) {
        if (dispatching_) throw std::logic_error("observe during dispatch");
        return reactor_->poll(timeout_ms);
    }
    void dispatch(const ReadyBatch& batch) {
        if (dispatching_) throw std::logic_error("recursive dispatch");
        struct Guard { bool& value; ~Guard() { value = false; } };
        dispatching_ = true; Guard reset{dispatching_};
        for (const auto& event : batch.events) {
            const auto it = std::find_if(nodes_.begin(), nodes_.end(), [&](const auto& n) {
                return n->id == event.id;
            });
            if (it == nodes_.end() || !reactor_->current(event.id)) continue;
            auto keep_alive = *it; // Copies ownership, not the callable's mutable state.
            keep_alive->handler(*this, keep_alive->socket, event);
        }
    }
 private:
    struct Node {
        Node(Socket s, Handler h) : socket(std::move(s)), handler(std::move(h)) {}
        Socket socket;
        Handler handler;
        Registration id = 0;
    };
    std::unique_ptr<StudyReactor> reactor_;
    std::vector<std::shared_ptr<Node>> nodes_;
    bool dispatching_ = false;
};
} // namespace study_net
#endif
