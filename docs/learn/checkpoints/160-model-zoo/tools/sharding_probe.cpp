#include "net/sharded_relay.h"
#include "net/transfer_inbox.h"
#include "net/reactor_choice.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
#include <exception>
#include <thread>
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
// Create before publishing, join before destruction. Only submit/stop cross threads.
class Shard {
public:
    explicit Shard(bool epoll) : loop_(choose_reactor(epoll)) {}
    ~Shard() { stop(); }
    bool submit(std::unique_ptr<RelayMatch>& match) {
        if (!inbox_.try_push(match)) return false;
        (void)loop_.wake(); // Work is already accepted. Periodic polling is the fallback.
        return true;
    }
    void start() { worker_ = std::thread([this] { run(); }); }
    void stop() {
        stopping_.store(true, std::memory_order_release);
        (void)loop_.wake();
        if (worker_.joinable()) worker_.join();
    }
    void verify(std::size_t expected) const {
        if (failure_) std::rethrow_exception(failure_); // Only after join.
        require(accepted_ == expected && rejected_ == 0, "shard admission counts");
    }
private:
    void run() noexcept {
        try {
            while (!stopping_.load(std::memory_order_acquire)) {
                auto batch = inbox_.take();
                for (auto& match : batch) if (match) {
                    if (loop_.attach(match)) ++accepted_;
                    else ++rejected_; // Failed attach retains match; batch destroys it outside lock.
                }
                loop_.tick(20);
            }
        } catch (...) { failure_ = std::current_exception(); }
        auto pending = inbox_.close_and_take(); // No later submit can succeed.
        loop_.clear();
        // pending objects return queue budgets and close sockets on this thread.
    }
    MatchLoop loop_;
    TransferInbox<RelayMatch, 4> inbox_;
    std::atomic<bool> stopping_{false};
    std::thread worker_;
    std::exception_ptr failure_;
    std::size_t accepted_ = 0, rejected_ = 0;
};
int main(int argc, char** argv) {
    Runtime runtime;
    if (!runtime.ready()) return 2;
    try {
        const bool epoll = epoll_argument(argc, argv);
        std::atomic<std::size_t> total{0}; // Declared before every possible owner.
        MatchLoop front(choose_reactor(epoll));
        Shard first(epoll), second(epoll);
        std::array<Shard*, 2> shards{&first, &second};
        std::array<Socket, 4> clients_a, clients_b;
        std::array<std::size_t, 4> received{};
        std::array<bool, 4> reverse{};
        for (std::size_t i = 0; i < 4; ++i) {
            auto a = pair(), b = pair();
            auto match = std::make_unique<RelayMatch>(static_cast<std::uint32_t>(i + 1),
                std::move(a.receiver), std::move(b.receiver), total);
            std::array<std::uint8_t, 1024> prefix{};
            for (std::size_t n = 0; n < prefix.size(); ++n) prefix[n] = (n + i) % 251;
            require(match->queues[1].append(prefix.data(), prefix.size()) == BufferResult::stored, "prefix");
            match->deadlines[1] = RelayMatch::Clock::now() + 3s;
            auto* identity = match.get();
            require(front.attach(match), "front owns pair");
            const auto stale = front.observe(0);
            match = front.detach(static_cast<std::uint32_t>(i + 1));
            front.dispatch(stale); // Returned events still exist; old registry rejects them.
            require(match.get() == identity && match->queues[1].paused(), "move graph, not its pointee");
            std::array<std::uint8_t, 64> tail{};
            for (std::size_t n = 0; n < tail.size(); ++n) tail[n] = (1024 + n + i) % 251;
            require(send_bounded(a.sender, tail.data(), tail.size(), RelayMatch::Clock::now() + 1s).outcome == SendOutcome::complete, "send during registration gap");
            send(b.sender, static_cast<std::uint8_t>(200 + i));
            require(shards[i % shards.size()]->submit(match) && !match, "round robin transfer");
            clients_a[i] = std::move(a.sender); clients_b[i] = std::move(b.sender);
        }
        require(front.size() == 0 && total == 4096, "front released matches, shared budget retained");
        first.start(); second.start();
        const auto stop = RelayMatch::Clock::now() + 5s;
        bool complete = false;
        while (!complete && RelayMatch::Clock::now() < stop) {
            complete = true;
            for (std::size_t i = 0; i < 4; ++i) {
                if (received[i] < 1088) {
                    std::uint8_t bytes[256];
                    const auto read = try_receive(clients_b[i], bytes, sizeof(bytes));
                    if (read.state == ReceiveState::progress) {
                        for (std::size_t n = 0; n < read.count; ++n)
                            require(bytes[n] == (received[i] + n + i) % 251, "per-match FIFO");
                        received[i] += read.count;
                        require(received[i] <= 1088, "unexpected extra bytes");
                    } else require(read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted, "destination remains connected");
                }
                if (!reverse[i]) {
                    std::uint8_t byte = 0;
                    const auto read = try_receive(clients_a[i], &byte, 1);
                    if (read.state == ReceiveState::progress) { require(byte == 200 + i, "reverse routing"); reverse[i] = true; }
                    else require(read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted, "reverse remains connected");
                }
                complete = complete && received[i] == 1088 && reverse[i];
            }
            std::this_thread::yield();
        }
        first.stop(); second.stop();
        first.verify(2); second.verify(2);
        require(complete && total == 0, "both shards delivered and returned reservations");
        std::unique_ptr<RelayMatch> empty;
        require(!first.submit(empty), "null rejected");
        auto a = pair(), b = pair();
        auto late = std::make_unique<RelayMatch>(9, std::move(a.receiver), std::move(b.receiver), total);
        require(!first.submit(late) && late, "closed shard retains caller ownership");
        std::puts("SHARDS: 4 matches / 2 owners; registration-gap data, pending FIFO, reverse traffic, stale batches and joined shutdown passed");
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
