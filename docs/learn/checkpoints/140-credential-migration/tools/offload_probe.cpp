#include "net/offload.h"
#include "net/timer_heap.h"
#include "net/reactor_choice.h"
#include "net/probe_frame.h"
#include "tests/reactor_fixture.h"
#include <future>
#include <cstdio>
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
struct ReleaseService {
    std::promise<void> promise;
    bool released = false;
    void finish() { if (!released) { promise.set_value(); released = true; } }
    ~ReleaseService() { finish(); } // Unblock jobs before pool destruction on failure.
};
int main(int argc, char** argv) {
    Runtime runtime; if (!runtime.ready()) return 2;
    try {
        CallbackLoop loop(choose_reactor(epoll_argument(argc, argv)));
        TimerHeap timers;
        std::promise<void> entered;
        bool received = false, timed_out = false, discarded = false, applied = false;
        Offload pool(2, [&] { require(loop.wake(), "wake failed"); }, 4);
        ReleaseService service;
        const auto release = service.promise.get_future().share();
        auto slow = pair(), fast = pair();
        FrameParser parser;
        const auto slow_id = loop.attach(std::move(slow.receiver), 0,
            [](CallbackLoop&, Socket&, const ReadyEvent&) {});
        const auto fast_id = loop.attach(std::move(fast.receiver), Read,
            [&](CallbackLoop&, Socket& socket, const ReadyEvent& event) {
                std::uint8_t bytes[16]; const auto read = try_receive(socket, bytes, sizeof(bytes));
                if (read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted) return;
                require(read.state == ReceiveState::progress && parser.append(bytes, read.count), "read");
                Frame frame; const auto parsed = parser.next(frame);
                if (parsed == ParseStatus::need_more) return;
                require(parsed == ParseStatus::frame && matches_probe(frame, 12, 7), "frame");
                received = true;
                const auto id = event.id; // ID and request value cross the worker boundary.
                const int request = 7;
                require(pool.submit([&, id, request] {
                    const int answer = request * 2;
                    return [&, id, answer] {
                        if (!loop.current(id)) return;
                        require(answer == 14, "owned result");
                        applied = true;
                        require(loop.close(id), "close fast");
                    };
                }), "submit fast");
                std::puts("I/O: another connection's complete frame handled while service is blocked");
            });
        require(slow_id && fast_id, "attach");
        const auto id = *slow_id;
        require(pool.submit([&, id, release] {
            entered.set_value();
            release.wait(); // Controlled blocking dependency, not event-loop work.
            const int answer = 21;
            return [&, id, answer] {
                if (!loop.current(id)) {
                    discarded = true;
                    std::puts("RESULT: removed registration; late answer discarded");
                    return;
                }
                require(answer == 21, "slow result");
            };
        }), "submit slow");
        // Test setup only: establish that the service is blocked before starting the loop.
        require(entered.get_future().wait_for(2s) == std::future_status::ready, "service start");
        EncodedFrame frame; require(encode_frame(probe_frame(12, 7), frame), "encode");
        require(send_bounded(fast.sender, frame.bytes.data(), frame.size,
            TimerHeap::Clock::now() + 2s).outcome == SendOutcome::complete, "send");
        timers.arm(*slow_id, TimerHeap::Clock::now() + 30ms);
        std::vector<Offload::Cont> completed;
        std::vector<TimerHeap::Token> due;
        const auto stop = TimerHeap::Clock::now() + 2s;
        while (!(received && timed_out && discarded && applied) && TimerHeap::Clock::now() < stop) {
            const int next = timers.timeout_ms(TimerHeap::Clock::now());
            const auto batch = loop.observe(next < 0 ? 50 : std::min(next, 50));
            require(batch.state != PollState::error, "poll");
            completed.clear(); pool.drain(completed);
            for (auto& cont : completed) cont(); // Loop owns state updates.
            loop.dispatch(batch); // A completion may have removed an ID from this batch.
            due.clear(); timers.expired(TimerHeap::Clock::now(), due);
            for (const auto token : due) {
                if (!loop.current(token)) continue;
                require(loop.close(token), "timeout closes slow");
                timed_out = true;
                service.finish(); // Let the already running service return a late result.
                std::puts("TIMER: slow connection removed before worker result returns");
            }
        }
        service.finish(); pool.shutdown();
        completed.clear(); pool.drain(completed);
        for (auto& cont : completed) cont();
        require(received && timed_out && discarded && applied, "offload loop result");
        require(pool.take_wake_errors() == 0, "wake failures");
        std::puts("LOOP: join and final drain completed while reactor and state are alive");
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
