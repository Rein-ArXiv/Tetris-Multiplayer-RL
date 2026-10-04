#include "net/offload.h"
#include <chrono>
#include <future>
#include <cstdio>
#include <stdexcept>
#include <string>
using study_net::Offload;
using namespace std::chrono_literals;
void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
struct WakeCount {
    std::mutex mutex;
    std::condition_variable changed;
    int count = 0;
    void notify() { { std::lock_guard<std::mutex> lock(mutex); ++count; } changed.notify_all(); }
    void wait(int target) {
        std::unique_lock<std::mutex> lock(mutex);
        require(changed.wait_for(lock, 2s, [&] { return count >= target; }), "completion deadline");
    }
};
// Release blocked jobs during exception unwinding before pool's destructor joins them.
struct Release {
    std::promise<void> promise;
    bool sent = false;
    void open() { if (!sent) { promise.set_value(); sent = true; } }
    ~Release() { open(); }
};
int main() {
    try {
        const auto owner = std::this_thread::get_id();
        WakeCount wakes;
        std::promise<void> entered;
        std::atomic<bool> worker_is_other{false};
        std::vector<int> applied;
        Offload pool(1, [&] { wakes.notify(); }, 2);
        Release release; auto gate = release.promise.get_future().share();
        require(pool.submit([&, gate] {
            worker_is_other = std::this_thread::get_id() != owner;
            entered.set_value(); gate.wait();
            return [&] { require(std::this_thread::get_id() == owner, "owner continuation"); applied.push_back(1); };
        }), "first accepted");
        require(entered.get_future().wait_for(2s) == std::future_status::ready, "worker entered");
        require(pool.submit([&] { return [&] { applied.push_back(2); }; }), "queued accepted");
        require(!pool.submit([] { return Offload::Cont{}; }), "waiting+running capacity");
        release.open(); wakes.wait(2);
        require(!pool.submit([] { return Offload::Cont{}; }), "completed but undrained still holds capacity");
        require(applied.empty(), "worker never runs continuation");
        std::vector<Offload::Cont> out{[&] { applied.push_back(0); }};
        require(pool.drain(out) == 2 && out.size() == 3, "drain appends");
        for (auto& cont : out) cont();
        out.clear();
        require(worker_is_other && applied == std::vector<int>({0, 1, 2}), "execution ownership");
        require(pool.submit([]()->Offload::Cont { throw std::runtime_error("expected job error"); }), "throwing job accepted");
        require(pool.submit([]()->Offload::Cont { throw std::logic_error("fallback"); }, [&] { applied.push_back(3); }), "fallback accepted");
        wakes.wait(4); require(pool.drain(out) == 2, "failures delivered");
        bool seen_error = false;
        for (auto& cont : out) {
            try { cont(); } catch (const std::runtime_error& e) {
                seen_error = std::string(e.what()) == "expected job error";
            }
        }
        require(seen_error && applied.back() == 3, "rethrow on owner and explicit fallback");
        require(!pool.submit({}), "empty job rejected");
        pool.shutdown(); pool.shutdown();
        require(!pool.submit([] { return Offload::Cont{}; }), "closed admission");
        {
            WakeCount completed;
            std::vector<int> order;
            Offload reordered(2, [&] { completed.notify(); }, 2);
            Release first; auto slow = first.promise.get_future().share();
            require(reordered.submit([&, slow] { slow.wait(); return [&] { order.push_back(1); }; }), "slow");
            require(reordered.submit([&] { return [&] { order.push_back(2); }; }), "fast");
            completed.wait(1); out.clear(); require(reordered.drain(out) == 1, "fast finishes first");
            out[0](); require(order == std::vector<int>{2}, "not submission order");
            first.open(); reordered.shutdown(); out.clear(); reordered.drain(out); out[0]();
            require(order == std::vector<int>({2, 1}), "completion order");
        }
        {
            int recovered = 0;
            Offload broken_wake(0, [] { throw std::runtime_error("wake failed"); }, 3);
            for (int i = 0; i < 3; ++i) require(broken_wake.submit([&] { return [&] { ++recovered; }; }), "accepted before shutdown");
            broken_wake.shutdown();
            require(broken_wake.take_wake_errors() == 3 && broken_wake.take_wake_errors() == 0, "wake error accounting");
            out.clear(); require(broken_wake.drain(out) == 3, "queued work retained after wake failure");
            for (auto& cont : out) cont();
            require(recovered == 3, "shutdown drains accepted jobs");
        }
        bool invalid = false;
        try { Offload none(1, {}, 0); } catch (const std::invalid_argument&) { invalid = true; }
        require(invalid, "zero capacity rejected");
        std::puts("OFFLOAD: capacity/publication/owner/failure/order/wake/shutdown contracts passed");
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
