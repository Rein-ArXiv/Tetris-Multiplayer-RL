#include "net/pair_queue.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <memory>
#include <thread>
#include <vector>

using namespace study_net;

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)

using Item = std::unique_ptr<unsigned>;
using Queue = PairQueue<Item, 4>;

auto keep = [](std::uint64_t, Item&) noexcept { return true; };

int main()
{
    static_assert(!std::is_copy_constructible_v<Queue>);
    unsigned rng = 77;
    for (unsigned trial = 0; trial < 64; ++trial)
    {
        Queue q;
        std::deque<unsigned> model;
        for (unsigned step = 0; step < 512; ++step)
        {
            rng = rng * 1664525u + 1013904223u;
            unsigned id = (rng >> 8) % 7, op = (rng >> 20) % 4;
            if (op == 0)
            {
                auto x = std::make_unique<unsigned>(id + 100);
                auto* before = x.get();
                const auto want = id == 0 ? QueueJoin::invalid_id :
                    std::find(model.begin(), model.end(), id) != model.end() ? QueueJoin::duplicate :
                    model.size() == 4 ? QueueJoin::full : QueueJoin::queued;
                CHECK(q.enqueue(id, x) == want);
                if (want == QueueJoin::queued)
                {
                    model.push_back(id);
                    CHECK(!x);
                }
                else
                    CHECK(x.get() == before && *x == id + 100);
            }
            else if (op == 1)
            {
                auto out = q.cancel(id);
                auto it = std::find(model.begin(), model.end(), id);
                CHECK(bool(out) == (it != model.end()));
                if (out)
                {
                    CHECK(out->id == id && *out->value == id + 100);
                    model.erase(it);
                }
            }
            else
            {
                const unsigned drop = op == 3 ? id : 0;
                model.erase(std::remove(model.begin(), model.end(), drop), model.end());
                auto out = q.try_pair([&](std::uint64_t key, Item&) noexcept { return key != drop; });
                CHECK(bool(out) == (model.size() >= 2));
                if (out)
                {
                    CHECK(out->first.id == model[0] && out->second.id == model[1]);
                    CHECK(*out->first.value == model[0] + 100 && *out->second.value == model[1] + 100);
                    model.pop_front();
                    model.pop_front();
                }
            }
            CHECK(q.size() == model.size() && !q.closed());
        }
        q.close();
        q.close();
        CHECK(q.closed() && q.size() == 0 && !q.try_pair(keep));
        auto x = std::make_unique<unsigned>(9);
        CHECK(q.enqueue(0, x) == QueueJoin::closed && *x == 9);
    }
    for (unsigned i = 0; i < 128; ++i)
    {
        Queue q;
        auto a = std::make_unique<unsigned>(101), b = std::make_unique<unsigned>(102);
        CHECK(q.enqueue(1, a) == QueueJoin::queued && q.enqueue(2, b) == QueueJoin::queued);
        std::atomic<bool> go{false};
        std::optional<Queue::Entry> cancelled;
        std::optional<Queue::Pair> paired;
        std::thread x([&] { while (!go.load()) std::this_thread::yield(); cancelled = q.cancel(1); });
        std::thread y([&] { while (!go.load()) std::this_thread::yield(); paired = q.try_pair(keep); });
        go = true;
        x.join();
        y.join();
        CHECK(bool(cancelled) != bool(paired));
        if (cancelled)
        {
            CHECK(cancelled->id == 1 && q.size() == 1);
            auto survivor = q.cancel(2);
            CHECK(survivor && survivor->id == 2);
        }
        else
            CHECK(paired->first.id == 1 && paired->second.id == 2 && q.size() == 0);
        q.close();
    }
    Queue q;
    auto x = std::make_unique<unsigned>(1);
    CHECK(q.enqueue(1, x) == QueueJoin::queued);
    std::atomic<bool> live{true};
    std::mutex mutex;
    std::condition_variable cv;
    bool seen = false, reaped = false;
    std::optional<Queue::Pair> result;
    std::thread consumer([&] {
        result = q.wait_pair([&](std::uint64_t, Item&) noexcept {
            const bool snapshot = live.load();
            std::lock_guard<std::mutex> lock(mutex);
            seen = true;
            if (!snapshot)
                reaped = true;
            cv.notify_all();
            return snapshot;
        });
    });
    bool first = false, second = false;
    {
        std::unique_lock<std::mutex> lock(mutex);
        first = cv.wait_for(lock, std::chrono::seconds(2), [&] { return seen; });
    }
    live = false; // No queue notification: the lone entry must be polled again.
    {
        std::unique_lock<std::mutex> lock(mutex);
        second = cv.wait_for(lock, std::chrono::seconds(2), [&] { return reaped; });
    }
    q.close();
    consumer.join();
    CHECK(first && second && !result && q.size() == 0);
    std::puts("pair queue: 32768 reference-model steps, 128 cancel/pair races, ownership preservation, fixed bound, lone polling and close wake");
}
