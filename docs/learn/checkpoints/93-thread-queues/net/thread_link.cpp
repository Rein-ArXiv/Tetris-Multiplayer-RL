#include "net/thread_link.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include "net/stream.h"
#include <chrono>
#include <utility>

namespace study_net {
ThreadLink::ThreadLink(Socket socket)
    : worker_([this, owned = std::move(socket)]() mutable {
          const LinkReport result = run(owned);
          owned.reset(); // Close in the owning thread, before publishing completion.
          outbound_.close();
          inbound_.close(); // Already queued frames can still be drained.
          std::lock_guard<std::mutex> lock(report_mutex_);
          report_ = result;
      }) {}

ThreadLink::~ThreadLink() {
    request_stop();
    // No queue/report lock held: the worker may need those locks to finish.
    if (worker_.joinable()) worker_.join();
}

std::optional<LinkReport> ThreadLink::report() const {
    std::lock_guard<std::mutex> lock(report_mutex_);
    return report_;
}

LinkReport ThreadLink::run(Socket& socket) {
    using Clock = std::chrono::steady_clock;
    int error = 0;
    if (!set_nonblocking(socket, true, error)) return {LinkEnd::io_error, error};
    FrameParser parser;
    EncodedFrame pending{};
    std::size_t offset = 0;
    bool sending = false, send_closed = false, peer_eof = false;
    Clock::time_point deadline{};
    while (!stop_.load(std::memory_order_relaxed)) {
        bool activity = false;
        // One frame remains in worker-local storage until its final byte sends.
        if (!sending && !send_closed) {
            Frame frame;
            const auto got = outbound_.try_pop(frame);
            if (got == QueueGet::item) {
                if (!encode_frame(frame, pending)) return {LinkEnd::protocol_error};
                offset = 0;
                sending = true;
                deadline = Clock::now() + std::chrono::seconds(5);
                activity = true;
            } else if (got == QueueGet::closed) {
                if (!shutdown_send(socket, error)) return {LinkEnd::io_error, error};
                send_closed = true;
                activity = true;
            }
        }
        if (sending) {
            if (Clock::now() >= deadline) return {LinkEnd::send_timeout};
            const auto sent = try_send(socket, pending.bytes.data() + offset,
                                       pending.size - offset);
            if (sent.state == SendState::error) return {LinkEnd::io_error, sent.error};
            if (sent.state == SendState::progress) {
                offset += sent.count;
                sending = offset != pending.size;
                activity = true;
            }
        }
        if (!peer_eof) {
            // Read more bytes only after next() reports need_more. Parse one
            // frame per turn so a receive burst does not monopolize the loop.
            Frame frame;
            const auto parsed = parser.next(frame);
            if (parsed == ParseStatus::error) return {LinkEnd::protocol_error};
            if (parsed == ParseStatus::frame) {
                if (inbound_.try_push(frame) != QueuePut::stored)
                    return {LinkEnd::receive_full};
                activity = true;
            } else {
                std::uint8_t bytes[16];
                const auto read = try_receive(socket, bytes, sizeof(bytes));
                if (read.state == ReceiveState::error) return {LinkEnd::io_error, read.error};
                if (read.state == ReceiveState::progress) {
                    if (!parser.append(bytes, read.count)) return {LinkEnd::protocol_error};
                    activity = true;
                } else if (read.state == ReceiveState::eof) {
                    if (parser.pending_bytes() != 0) return {LinkEnd::truncated};
                    peer_eof = true;
                    activity = true;
                }
            }
        }
        if (send_closed && peer_eof) return {LinkEnd::complete};
        // Cooperative polling: no hard latency guarantee from the OS scheduler.
        if (!activity) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return {LinkEnd::cancelled}; // Abort may leave unsent frames; never report complete.
}
} // namespace study_net
