#pragma once
#include "net/pending_send.h"
#include "net/study_reactor.h"
#include "net/receive_socket.h"
#include "net/send_socket.h"
#include "net/timer_heap.h"
#include <array>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <utility>
namespace study_net {
// This graph stays at one address. Only its unique_ptr crosses loop boundaries.
// The shared byte counter must outlive all matches, including inbox entries.
struct RelayMatch {
    using Clock = TimerHeap::Clock;
    using Queue = PendingSend<4096, 1024, 256>;
    RelayMatch(std::uint32_t number, Socket a, Socket b,
               std::atomic<std::size_t>& total)
        : id(number), sockets{std::move(a), std::move(b)},
          queues{Queue(total, 65536), Queue(total, 65536)} {
        if (!id) throw std::invalid_argument("nonzero match identity");
    }
    const std::uint32_t id;
    std::array<Socket, 2> sockets;
    std::array<Queue, 2> queues;
    std::array<std::optional<Clock::time_point>, 2> deadlines{};
    bool closed = false;
};

// One owner calls every method except wake(). Socket registration identities
// live here, not in the transferable match. A pair occupies two registrations.
class MatchLoop {
public:
    using Clock = RelayMatch::Clock;
    static constexpr std::size_t capacity = Registrations::capacity / 2;
    explicit MatchLoop(std::unique_ptr<StudyReactor> reactor,
                       std::chrono::milliseconds stall = std::chrono::seconds(2))
        : reactor_(std::move(reactor)), stall_(stall) {
        if (!reactor_ || stall_.count() <= 0) throw std::invalid_argument("loop setup");
    }
    ~MatchLoop() { clear(); }
    MatchLoop(const MatchLoop&) = delete;
    MatchLoop& operator=(const MatchLoop&) = delete;

    // Failure retains the caller's match. Roll back a successful first watch.
    bool attach(std::unique_ptr<RelayMatch>& match) {
        if (dispatching_) throw std::logic_error("attach during dispatch");
        if (!reactor_ || !match || match->closed || find(match->id)) return false;
        Entry* slot = nullptr;
        for (auto& entry : entries_) if (!entry.match) { slot = &entry; break; }
        if (!slot) return false;
        std::array<Registration, 2> ids{};
        std::array<unsigned, 2> interests{};
        for (std::size_t side = 0; side < 2; ++side) {
            int error = 0;
            if (!set_nonblocking(match->sockets[side], true, error)) {
                if (ids[0] && !reactor_->unwatch(ids[0])) invalidate();
                return false;
            }
            interests[side] = desired(*match, side);
            const auto id = reactor_->watch(match->sockets[side], interests[side]);
            if (!id) {
                if (ids[0] && !reactor_->unwatch(ids[0])) invalidate();
                return false;
            }
            ids[side] = *id;
        }
        slot->ids = ids;
        slot->interests = interests;
        slot->match = std::move(match);
        return true;
    }

    // Only at a dispatch boundary: no active handler may retain a reference.
    std::unique_ptr<RelayMatch> detach(std::uint32_t match_id) {
        if (dispatching_) throw std::logic_error("detach during dispatch");
        auto* entry = find(match_id);
        if (!entry) return {};
        for (const auto id : entry->ids) {
            if (!reactor_->unwatch(id)) { invalidate(); return {}; }
        }
        entry->ids = {};
        entry->interests = {};
        return std::move(entry->match); // Queues, pause flags and deadlines remain intact.
    }
    bool contains(std::uint32_t match_id) const {
        for (const auto& entry : entries_)
            if (entry.match && entry.match->id == match_id) return true;
        return false;
    }
    std::size_t size() const {
        std::size_t count = 0;
        for (const auto& entry : entries_) if (entry.match) ++count;
        return count;
    }
    bool wake() noexcept { return reactor_ && reactor_->wake(); }
    ReadyBatch observe(int wait_ms) {
        if (dispatching_) throw std::logic_error("observe during dispatch");
        if (!reactor_) throw std::runtime_error("reactor invalidated");
        int wait = (std::max)(0, (std::min)(wait_ms, 500));
        const auto now = Clock::now();
        for (const auto& entry : entries_) if (entry.match)
            for (const auto& deadline : entry.match->deadlines) if (deadline)
                wait = (std::min)(wait, deadline_wait_ms(now, *deadline));
        return reactor_->poll(wait);
    }
    // The batch must come from this loop. Numeric IDs can coincide across loops.
    void dispatch(const ReadyBatch& batch) {
        if (dispatching_) throw std::logic_error("recursive dispatch");
        if (!reactor_) throw std::runtime_error("reactor invalidated");
        if (batch.state == PollState::error) { invalidate(); throw std::runtime_error("poll failure"); }
        dispatching_ = true;
        struct Guard { bool& active; ~Guard() { active = false; } } guard{dispatching_};
        for (const auto& event : batch.events) {
            if (!reactor_) break;
            if (!reactor_->current(event.id)) continue;
            for (auto& entry : entries_) if (entry.match && !entry.match->closed)
                for (std::size_t side = 0; side < 2; ++side)
                    if (entry.match && entry.ids[side] == event.id) on_event(entry, side, event);
        }
        // I/O before expiry is the chosen service policy. A transfer never resets due time.
        const auto now = Clock::now();
        for (auto& entry : entries_) if (entry.match && !entry.match->closed)
            for (const auto& deadline : entry.match->deadlines)
                if (deadline && *deadline <= now) { close(entry); break; }
        for (auto& entry : entries_) if (entry.match && entry.match->closed) entry.match.reset();
    }
    void tick(int wait_ms = 5) { dispatch(observe(wait_ms)); }
    void clear() noexcept {
        for (auto& entry : entries_) if (entry.match) { close(entry); entry.match.reset(); }
    }
private:
    // Failed unregistration leaves backend ownership uncertain. Destroy its entire
    // interest set before closing any socket; this loop cannot be used again.
    void invalidate() noexcept {
        reactor_.reset();
        for (auto& entry : entries_) {
            entry.ids = {};
            entry.match.reset();
        }
    }
    struct Entry {
        std::unique_ptr<RelayMatch> match;
        std::array<Registration, 2> ids{};
        std::array<unsigned, 2> interests{};
    };
    Entry* find(std::uint32_t id) {
        for (auto& entry : entries_) if (entry.match && entry.match->id == id) return &entry;
        return nullptr;
    }
    static unsigned desired(const RelayMatch& match, std::size_t side) {
        const auto& destination = match.queues[1 - side];
        return (!destination.paused() && destination.room() >= 16 ? unsigned(Read) : 0u) |
               (match.queues[side].size() ? unsigned(Write) : 0u);
    }
    void close(Entry& entry) noexcept {
        auto& match = *entry.match;
        if (match.closed) return;
        match.closed = true;
        for (std::size_t side = 0; side < 2; ++side) {
            if (!reactor_->unwatch(entry.ids[side])) { invalidate(); return; }
            match.queues[side].close();
            match.deadlines[side].reset();
        }
        entry.ids = {};
        // Socket objects stay alive until the current batch finishes using Entry.
    }
    bool sync(Entry& entry) {
        for (std::size_t side = 0; side < 2; ++side) {
            const unsigned flags = desired(*entry.match, side);
            if (flags == entry.interests[side]) continue;
            if (!reactor_->change(entry.ids[side], flags)) { close(entry); return false; }
            entry.interests[side] = flags;
        }
        return true;
    }
    void on_event(Entry& entry, std::size_t side, const ReadyEvent& event) {
        auto& match = *entry.match;
        auto& outbound = match.queues[side];
        if (event.writable && outbound.size()) {
            const auto result = outbound.flush([&](const std::uint8_t* data, std::size_t count) noexcept {
                return try_send(match.sockets[side], data, count);
            });
            if (result == FlushResult::error || result == FlushResult::invalid) { close(entry); return; }
            if (result == FlushResult::progress) {
                if (outbound.size()) match.deadlines[side] = Clock::now() + stall_;
                else match.deadlines[side].reset();
            }
            if (!sync(entry)) return;
        }
        if (!(event.readable || event.error)) return;
        auto& destination = match.queues[1 - side];
        if (destination.paused()) { if (event.error) close(entry); return; }
        std::uint8_t bytes[16];
        const auto read = try_receive(match.sockets[side], bytes, sizeof(bytes));
        if (read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted) return;
        if (read.state != ReceiveState::progress) { close(entry); return; }
        const bool empty = destination.size() == 0;
        if (destination.append(bytes, read.count) != BufferResult::stored) { close(entry); return; }
        if (empty) match.deadlines[1 - side] = Clock::now() + stall_;
        sync(entry);
    }
    std::unique_ptr<StudyReactor> reactor_;
    std::array<Entry, capacity> entries_{};
    std::chrono::milliseconds stall_;
    bool dispatching_ = false;
};
} // namespace study_net
