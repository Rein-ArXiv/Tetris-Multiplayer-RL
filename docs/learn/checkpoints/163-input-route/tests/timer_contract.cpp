#include "net/timer_heap.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <tuple>
using namespace study_net;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        using T = TimerHeap::TimePoint;
        using D = TimerHeap::Clock::duration;
        using ms = std::chrono::milliseconds;
        require(deadline_wait_ms(T::min(), T::max()) == kMaxTimerWaitMs, "saturate before subtract");
        require(deadline_wait_ms(T::max(), T::min()) == 0, "past extremes");
        require(deadline_wait_ms(T::max() - D(1), T::max()) == 1, "one tick near max");
        require(deadline_wait_ms(T{}, T{} + D(1)) == 1, "future rounds up");
        require(deadline_wait_ms(T{}, T{} + ms(1)) == 1, "exact millisecond");
        require(deadline_wait_ms(T{}, T{} + ms(1) + D(1)) == 2, "fraction rounds up");
        require(deadline_wait_ms(T{}, T{}) == 0, "equal now");
        TimerHeap timer;
        bool invalid = false;
        try { timer.arm(0, T{}); } catch (const std::invalid_argument&) { invalid = true; }
        require(invalid && timer.empty(), "reject zero token");
        timer.arm(1, T{} + ms(1000)); timer.arm(1, T{} + ms(100));
        std::vector<TimerHeap::Token> out;
        timer.expired(T{} + ms(150), out);
        require(out == std::vector<TimerHeap::Token>{1}, "first generation");
        out.clear(); timer.arm(1, T{} + ms(2000));
        timer.expired(T{} + ms(1000), out); require(out.empty(), "old generation rejected");
        timer.expired(T{} + ms(2000), out); require(out.size() == 1, "new generation survives");
        timer.arm(1, T{}); timer.arm(2, T{}); timer.arm(1, T{}); timer.arm(3, T{});
        out = {99}; timer.expired(T{}, out);
        require(out == std::vector<TimerHeap::Token>({99, 2, 1, 3}), "append and tie order");
        timer.cancel(1); require(out.size() == 4, "delivered snapshot remains");
        timer.arm(7, T{} + ms(10)); timer.cancel(7);
        require(timer.timeout_ms(T{}) == -1, "stale top pruned before deadline");
        // Reference model stores only current deadlines and explicitly sorts due items.
        using Item = std::pair<T, std::uint64_t>;
        std::map<TimerHeap::Token, Item> live;
        std::uint64_t sequence = 0, random = 2463534242;
        auto rng = [&] { random ^= random << 13; random ^= random >> 7; random ^= random << 17; return random; };
        for (int step = 0; step < 30000; ++step) {
            const auto token = rng() % 32 + 1;
            const auto now = T{} + ms(step / 3);
            switch (rng() % 3) {
            case 0: { const auto when = now + ms(rng() % 100);
                      timer.arm(token, when); live[token] = {when, ++sequence}; break; }
            case 1: timer.cancel(token); live.erase(token); break;
            default: {
                std::vector<std::tuple<T, std::uint64_t, TimerHeap::Token>> expected;
                for (const auto& item : live) if (item.second.first <= now)
                    expected.emplace_back(item.second.first, item.second.second, item.first);
                std::sort(expected.begin(), expected.end()); out.clear(); timer.expired(now, out);
                require(out.size() == expected.size(), "model due size");
                for (std::size_t i = 0; i < out.size(); ++i) {
                    require(out[i] == std::get<2>(expected[i]), "model due identity/order"); live.erase(out[i]);
                }
            }}
            int wait = -1;
            for (const auto& item : live) {
                const int candidate = deadline_wait_ms(now, item.second.first);
                if (wait < 0 || candidate < wait) wait = candidate;
            }
            require(timer.timeout_ms(now) == wait, "model earliest deadline");
            require(timer.empty() == live.empty(), "model live emptiness");
        }
        std::puts("TIMER: extremes/ceil/ties/rearm/cancel/ABA; 30000 model operations passed");
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
