#include "net/pending_send.h"
#include <thread>
#include <vector>
#include <limits>
#include <stdexcept>
#include <cstdio>
using namespace study_net;
void check(bool ok) { if (!ok) throw std::runtime_error("backpressure contract"); }
int main() {
    std::atomic<std::size_t> total{7};
    std::size_t committed = 91;
    check(try_reserve_bytes(total, 10, 2, committed) && committed == 9 && total == 9);
    check(!try_reserve_bytes(total, 10, 2, committed) && committed == 9 && total == 9);
    check(!try_reserve_bytes(total, 8, 0, committed));
    total = 1;
    check(!try_reserve_bytes(total, SIZE_MAX, SIZE_MAX, committed) && total == 1);
    total = 0;
    check(try_reserve_bytes(total, SIZE_MAX, SIZE_MAX, committed) && committed == SIZE_MAX);
    total = 0;
    std::atomic<bool> bad{false};
    std::vector<std::thread> threads;
    for (int n = 0; n < 8; ++n) threads.emplace_back([&] {
        for (int i = 0; i < 1000; ++i) {
            std::size_t count = 999;
            if (try_reserve_bytes(total, 11, 3, count)) {
                if (count > 11 || count < 3) bad = true;
                std::this_thread::yield();
                total.fetch_sub(3, std::memory_order_relaxed);
            } else if (count != 999) bad = true;
        }
    });
    for (auto& thread : threads) thread.join();
    check(!bad && total == 0);
    const std::uint8_t data[]{1,2,3,4,5,6,7,8};
    {
        PendingSend<8,6,2> queue(total, 10), other(total, 10);
        check(queue.append(data,6) == BufferResult::stored && queue.paused() && total == 6);
        check(queue.flush([](auto*,auto) noexcept { return SendAttempt{SendState::would_block,0,0}; }) == FlushResult::waiting);
        check(total == 6 && queue.size() == 6);
        check(other.append(data,5) == BufferResult::global_limit && total == 6);
        check(queue.append(data,3) == BufferResult::local_limit && total == 6);
        check(queue.flush([](auto*,auto) noexcept { return SendAttempt{SendState::progress,3,0}; }) == FlushResult::progress);
        check(queue.size() == 3 && queue.paused() && total == 3);
        check(queue.flush([](auto*,auto) noexcept { return SendAttempt{SendState::progress,1,0}; }) == FlushResult::progress);
        check(queue.size() == 2 && !queue.paused() && total == 2);
        check(queue.append(data,6) == BufferResult::stored); // Ring wraps; oldest 5,6 stay first.
        std::vector<std::uint8_t> received;
        received.reserve(8);
        while (queue.size()) {
            check(queue.flush([&](const std::uint8_t* p, std::size_t n) noexcept {
                const auto accepted = (std::min)(n, std::size_t{3});
                for (std::size_t i = 0; i < accepted; ++i) received.push_back(p[i]);
                return SendAttempt{SendState::progress,accepted,0};
            }) == FlushResult::progress);
        }
        check(received == std::vector<std::uint8_t>({5,6,1,2,3,4,5,6}) && total == 0);
        check(queue.append(data,3) == BufferResult::stored);
        check(queue.flush([](auto*,auto) noexcept { return SendAttempt{SendState::progress,0,0}; }) == FlushResult::invalid);
        check(queue.size() == 3 && total == 3);
        queue.close(); queue.close();
        check(total == 0 && queue.append(data,1) == BufferResult::closed);
        check(other.append(data,4) == BufferResult::stored && total == 4);
    }
    check(total == 0); // Destructor discards the last queue exactly once.
    std::puts("budget CAS/overflow, hysteresis, wrap/partial order, waiting, invalid progress, discard passed");
}
