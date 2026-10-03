#ifndef STUDY_NET_THREAD_LINK_H
#define STUDY_NET_THREAD_LINK_H
#include "net/flow_queue.h"
#include "net/socket.h"
#include "net/heartbeat.h"
#include <atomic>
#include <optional>
#include <thread>

namespace study_net {
enum class LinkEnd { complete, cancelled, io_error, protocol_error,
                     truncated, receive_full, send_timeout, heartbeat_timeout };
struct LinkReport { LinkEnd end; int error = 0; };

// The caller transfers a connected socket. Runtime must outlive this object.
// The worker alone owns/accesses the socket and parser. Main owns game state.
// Public calls belong to one controlling thread; destruction joins the worker.
// Queue acceptance means local storage, not delivery or peer application.
class ThreadLink {
 public:
    explicit ThreadLink(Socket socket, bool heartbeat = false, BeatTiming timing = {});
    std::uint64_t confirmed_pongs() const noexcept { return pongs_.load(); }
    ~ThreadLink();
    ThreadLink(const ThreadLink&) = delete;
    ThreadLink& operator=(const ThreadLink&) = delete;
    QueuePut send(const Frame& frame) { return outbound_.try_push(frame); }
    FlowStats send_stats() const { return outbound_.stats(); }
    QueueGet receive(Frame& frame) { return inbound_.try_pop(frame); }
    void finish_sending() { outbound_.close(); }
    void request_stop() noexcept { stop_.store(true, std::memory_order_relaxed); }
    std::optional<LinkReport> report() const;
 private:
    LinkReport run(Socket& socket, bool heartbeat, BeatTiming timing);
    FlowQueue<8, 140, 105, 70> outbound_;
    FrameQueue<8> inbound_;
    std::atomic_bool stop_{false};
    std::atomic<std::uint64_t> pongs_{0};
    mutable std::mutex report_mutex_;
    std::optional<LinkReport> report_;
    // Last: all state exists before the new thread can use this.
    std::thread worker_;
};
} // namespace study_net
#endif
