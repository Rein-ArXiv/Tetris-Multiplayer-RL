#pragma once
#include "net/pending_send.h"
#include "net/callback_loop.h"
#include "net/receive_socket.h"
#include "net/timer_heap.h"
#include <array>
#include <chrono>
#include <stdexcept>
namespace study_net {
// Post-admission duplex byte relay. No interpretation or alteration of frames.
// One pair owns two registrations. Any terminal failure closes the whole pair.
class BufferedRelay {
public:
    using Clock = TimerHeap::Clock;
    using Queue = PendingSend<4096, 1024, 256>;
    explicit BufferedRelay(std::unique_ptr<StudyReactor> reactor,
                           std::chrono::milliseconds stall = std::chrono::seconds(2))
        : loop_(std::move(reactor)), queues_{Queue(total_, 8192), Queue(total_, 8192)},
          stall_(stall) {
        if (stall_.count() <= 0) throw std::invalid_argument("positive stall deadline");
    }
    ~BufferedRelay() { close(); }
    void attach(Socket a, Socket b) {
        if (closed_ || attached_) throw std::logic_error("pair already attached");
        attached_ = true;
        std::array<Socket, 2> sockets{std::move(a), std::move(b)};
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = loop_.attach(std::move(sockets[side]), Read,
                [this, side](CallbackLoop&, Socket& socket, const ReadyEvent& event) {
                    on_event(side, socket, event);
                });
            if (!id) { close(); throw std::runtime_error("attach failed"); }
            ids_[side] = *id;
            interests_[side] = Read;
        }
    }
    void tick(int wait_ms = 5) {
        if (closed_) return;
        const int timer = timers_.timeout_ms(Clock::now());
        const auto batch = loop_.observe(timer < 0 ? wait_ms : (std::min)(wait_ms, timer));
        if (batch.state == PollState::error) { close(); return; }
        loop_.dispatch(batch);
        std::vector<TimerHeap::Token> due;
        timers_.expired(Clock::now(), due);
        if (!due.empty()) { ++stalls_; close(); }
    }
    void close() noexcept {
        if (closed_) return;
        closed_ = true;
        for (std::size_t side = 0; side < 2; ++side) {
            timers_.cancel(ids_[side]);
            if (ids_[side]) loop_.close(ids_[side]);
            queues_[side].close();
        }
    }
    bool closed() const noexcept { return closed_; }
    bool paused(std::size_t source) const { return queues_.at(1 - source).paused(); }
    std::size_t pending() const noexcept { return total_.load(std::memory_order_relaxed); }
    std::size_t pauses() const noexcept { return pauses_; }
    std::size_t resumes() const noexcept { return resumes_; }
    std::size_t stalls() const noexcept { return stalls_; }
private:
    bool sync() {
        for (std::size_t side = 0; side < 2; ++side) {
            const bool read = !queues_[1 - side].paused() && queues_[1 - side].room() >= 16;
            const unsigned desired = (read ? static_cast<unsigned>(Read) : 0u) |
                (queues_[side].size() ? static_cast<unsigned>(Write) : 0u);
            if (desired == interests_[side]) continue;
            if (!loop_.change(ids_[side], desired)) { close(); return false; }
            const bool was_reading = (interests_[side] & Read) != 0;
            if (was_reading && !read) ++pauses_;
            if (!was_reading && read) ++resumes_;
            interests_[side] = desired;
        }
        return true;
    }
    void on_event(std::size_t side, Socket& socket, const ReadyEvent& event) {
        if (closed_) return;
        auto& outbound = queues_[side];
        if (event.writable && outbound.size()) {
            const auto result = outbound.flush([&](const std::uint8_t* data, std::size_t size) noexcept {
                return try_send(socket, data, size);
            });
            if (result == FlushResult::error || result == FlushResult::invalid) { close(); return; }
            if (result == FlushResult::progress) {
                if (outbound.size()) timers_.arm(ids_[side], Clock::now() + stall_);
                else timers_.cancel(ids_[side]);
            }
            if (!sync()) return;
        }
        // Observation preceded dispatch: another event may have paused this source.
        auto& destination = queues_[1 - side];
        if (!(event.readable || event.error)) return;
        if (destination.paused()) {
            if (event.error) close(); // HUP/ERR can be reported even with Read disabled.
            return;
        }
        std::uint8_t bytes[16];
        const auto read = try_receive(socket, bytes, sizeof(bytes));
        if (read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted) return;
        if (read.state != ReceiveState::progress) { close(); return; }
        const bool was_empty = destination.size() == 0;
        if (destination.append(bytes, read.count) != BufferResult::stored) { close(); return; }
        if (was_empty) timers_.arm(ids_[1 - side], Clock::now() + stall_);
        sync();
    }
    std::atomic<std::size_t> total_{0}; // Outlives the queues that return reservations.
    CallbackLoop loop_;
    TimerHeap timers_;
    std::array<Queue, 2> queues_;
    std::array<Registration, 2> ids_{};
    std::array<unsigned, 2> interests_{};
    std::chrono::milliseconds stall_;
    bool closed_ = false, attached_ = false;
    std::size_t pauses_ = 0, resumes_ = 0, stalls_ = 0;
};
} // namespace study_net
