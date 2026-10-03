#ifndef STUDY_NET_POSTED_RECEIVE_H
#define STUDY_NET_POSTED_RECEIVE_H
#include "net/read_hint.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <stdexcept>
#include <utility>

namespace study_net {
enum class CompletionState { data, eof, cancelled, timeout, error };
struct ReadCompletion {
    std::uint64_t id = 0;
    CompletionState state = CompletionState::error;
    std::size_t count = 0;
    int error = 0;
    std::array<std::uint8_t, 16> bytes{};
};

// Worker-backed completion API for this lesson, NOT native IOCP/io_uring.
// Owns a connected socket; Runtime must outlive it. One controlling thread.
// One posted read at a time, including a finished result not yet collected.
class PostedReceiver {
 public:
    explicit PostedReceiver(Socket socket) : socket_(std::move(socket)) {
        int error = 0;
        if (!set_nonblocking(socket_, true, error))
            throw std::runtime_error("nonblocking receive setup");
    }
    PostedReceiver(const PostedReceiver&) = delete;
    PostedReceiver& operator=(const PostedReceiver&) = delete;
    PostedReceiver(PostedReceiver&&) = delete;
    PostedReceiver& operator=(PostedReceiver&&) = delete;
    ~PostedReceiver() {
        request_cancel();
        // Request != completion: keep socket and buffer alive until execution ends.
        if (pending_.valid()) pending_.wait();
    }
    bool post(std::uint64_t id, std::size_t capacity, std::chrono::milliseconds timeout) {
        if (pending_.valid() || capacity == 0 || capacity > buffer_.size() ||
            timeout < std::chrono::milliseconds(1) || timeout > std::chrono::seconds(5))
            return false;
        cancel_.store(false, std::memory_order_relaxed);
        buffer_.fill(0);
        const auto deadline = Clock::now() + timeout;
        pending_ = std::async(std::launch::async, [this, id, capacity, deadline] {
            return run(id, capacity, deadline);
        });
        return true;
    }
    void request_cancel() noexcept { cancel_.store(true, std::memory_order_relaxed); }
    bool ready() const {
        return pending_.valid() &&
               pending_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    }
    std::optional<ReadCompletion> take() {
        if (!ready()) return std::nullopt;
        return pending_.get(); // Exactly once; next post is permitted afterwards.
    }
 private:
    using Clock = std::chrono::steady_clock;
    ReadCompletion run(std::uint64_t id, std::size_t capacity, Clock::time_point deadline) {
        const auto finish = [&](CompletionState state, std::size_t count = 0, int error = 0) {
            return ReadCompletion{id, state, count, error, buffer_};
        };
        for (;;) {
            if (cancel_.load(std::memory_order_relaxed)) return finish(CompletionState::cancelled);
            if (Clock::now() >= deadline) return finish(CompletionState::timeout);
            // A finite wait lets this teaching worker observe cancellation/deadline.
            // It is not a hard bound on when the OS schedules the worker again.
            const auto hint = wait_readable(socket_, 20);
            if (hint.state == WaitState::error) return finish(CompletionState::error, 0, hint.error);
            if (hint.state != WaitState::ready) continue;
            if (Clock::now() >= deadline) return finish(CompletionState::timeout);
            const auto attempt = try_receive(socket_, buffer_.data(), capacity);
            if (attempt.state == ReceiveState::progress)
                return finish(CompletionState::data, attempt.count);
            if (attempt.state == ReceiveState::eof) return finish(CompletionState::eof);
            if (attempt.state == ReceiveState::error)
                return finish(CompletionState::error, 0, attempt.error);
            // would_block/interrupted: recheck cancellation and the same deadline.
        }
    }
    Socket socket_;
    std::array<std::uint8_t, 16> buffer_{};
    std::atomic_bool cancel_{false};
    std::future<ReadCompletion> pending_; // Last: execution ends before borrowed state dies.
};
} // namespace study_net
#endif
