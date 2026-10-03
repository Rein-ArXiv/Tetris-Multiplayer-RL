#include "net/timer_heap.h"
#include "net/callback_loop.h"
#include "net/probe_frame.h"
#include "net/receive_socket.h"
#include "net/reactor_choice.h"
#include "tests/reactor_fixture.h"
#include <cstdio>
using namespace study_net;
using namespace study_fixture;
int main(int argc, char** argv) {
    Runtime runtime; if (!runtime.ready()) return 2;
    try {
        CallbackLoop loop(choose_reactor(epoll_argument(argc, argv)));
        TimerHeap timers;
        auto sockets = pair();
        FrameParser parser;
        bool received = false, timed_out = false;
        TimerHeap::TimePoint idle_deadline{};
        const auto id = loop.attach(std::move(sockets.receiver), Read,
            [&](CallbackLoop&, Socket& socket, const ReadyEvent& event) {
                std::uint8_t bytes[16];
                const auto result = try_receive(socket, bytes, sizeof(bytes));
                if (result.state == ReceiveState::would_block || result.state == ReceiveState::interrupted) return;
                require(result.state == ReceiveState::progress && parser.append(bytes, result.count), "receive");
                Frame frame;
                const auto parsed = parser.next(frame);
                if (parsed == ParseStatus::need_more) return;
                require(parsed == ParseStatus::frame && matches_probe(frame, 12, 7), "frame");
                received = true;
                idle_deadline = TimerHeap::Clock::now() + std::chrono::milliseconds(30);
                timers.arm(event.id, idle_deadline);
                std::puts("FRAME: received; first-frame deadline replaced by idle deadline");
            });
        require(bool(id), "attach");
        timers.arm(*id, TimerHeap::Clock::now() + std::chrono::seconds(1));
        EncodedFrame frame; require(encode_frame(probe_frame(12, 7), frame), "encode");
        require(send_bounded(sockets.sender, frame.bytes.data(), frame.size,
            TimerHeap::Clock::now() + std::chrono::seconds(2)).outcome == SendOutcome::complete, "send");
        require(loop.wake(), "wake"); // Early return still requires a fresh clock sample.
        std::vector<TimerHeap::Token> due;
        const auto stop = TimerHeap::Clock::now() + std::chrono::seconds(2);
        while (!timed_out && TimerHeap::Clock::now() < stop) {
            const auto before_wait = TimerHeap::Clock::now();
            const int next = timers.timeout_ms(before_wait);
            const int wait = next < 0 ? 100 : std::min(next, 100);
            const auto batch = loop.observe(wait);
            require(batch.state != PollState::error, "observe");
            loop.dispatch(batch); // Application policy: process observed input before expiry.
            due.clear();
            timers.expired(TimerHeap::Clock::now(), due);
            for (const auto token : due) {
                if (!loop.current(token)) continue;
                require(received && TimerHeap::Clock::now() >= idle_deadline, "not early");
                timers.cancel(token);
                require(loop.close(token), "timeout closes registration before socket destruction");
                timed_out = true;
                std::puts("TIMER: idle deadline reached; connection removed once");
            }
        }
        require(received && timed_out && timers.empty() && !loop.current(*id), "loop result");
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
