#include "net/admission_session.h"
#include "net/offload.h"
#include "net/timer_heap.h"
#include "net/reactor_choice.h"
#include "net/probe_frame.h"
#include "tests/reactor_fixture.h"
#include <unordered_map>
#include <cstdio>
using namespace study_net;
using namespace study_fixture;
using namespace std::chrono_literals;
struct SessionLoop {
    CallbackLoop loop;
    TimerHeap timers;
    std::unordered_map<Registration, AdmissionSession> sessions;
    Offload pool;
    int games = 0, ignored = 0, timed_out = 0;
    explicit SessionLoop(bool epoll) : loop(choose_reactor(epoll)),
        pool(1, [this] { require(loop.wake(), "wake"); }, 4) {}
    void close(Registration id) {
        auto it = sessions.find(id);
        if (it == sessions.end()) return;
        it->second.close();
        timers.cancel(id);
        require(loop.close(id), "unwatch");
        sessions.erase(it);
    }
    bool sync_interest(Registration id) {
        auto it = sessions.find(id);
        if (it == sessions.end()) return false;
        if (!loop.change(id, it->second.can_read() ? Read : 0)) {
            close(id);
            return false;
        }
        return true;
    }
    void apply_auth(Registration id, bool accepted) {
        auto it = sessions.find(id);
        if (it == sessions.end() || !it->second.resume_auth(id, accepted)) {
            ++ignored;
            return;
        }
        timers.arm(id, TimerHeap::Clock::now() + 1s);
        if (sync_interest(id)) pump(id); // Existing bytes need no new OS readiness event.
    }
    void pair_ready() {
        std::vector<Registration> ready;
        for (const auto& entry : sessions) if (entry.second.ready()) ready.push_back(entry.first);
        if (ready.size() < 2) return;
        for (std::size_t i = 0; i < 2; ++i) {
            const auto id = ready[i];
            require(sessions.at(id).begin_forward(), "transition");
            timers.arm(id, TimerHeap::Clock::now() + 1s);
        }
        for (std::size_t i = 0; i < 2; ++i) if (sync_interest(ready[i])) pump(ready[i]);
    }
    void pump(Registration id) {
        for (;;) {
            auto it = sessions.find(id);
            if (it == sessions.end()) return;
            Frame frame;
            const auto effect = it->second.step(frame);
            using E = AdmissionSession::Effect;
            if (effect == E::none) {
                sync_interest(id);
                return;
            }
            if (effect == E::closed) {
                close(id);
                return;
            }
            if (effect == E::authenticate) {
                const auto request = it->second.request();
                timers.arm(id, TimerHeap::Clock::now() + 1s);
                if (!sync_interest(id)) return;
                const bool submitted = pool.submit([this, id, request] {
                    // Controlled service stand-in; no Conn/Session reference crosses threads.
                    const bool accepted = request.route == AdmissionRoute::queue;
                    return [this, id, accepted] { apply_auth(id, accepted); };
                }, [this, id] { apply_auth(id, false); });
                if (!submitted) close(id);
                return;
            }
            if (effect == E::ready) {
                sync_interest(id);
                pair_ready();
                return;
            }
            require(matches_probe(frame, 3, 9), "game payload");
            ++games;
            close(id);
            return; // This probe finishes after one game message per side.
        }
    }
    Registration attach(Socket socket) {
        const auto id = loop.attach(std::move(socket), Read,
            [this](CallbackLoop&, Socket& socket, const ReadyEvent& event) {
                const auto it = sessions.find(event.id);
                if (it == sessions.end() || !it->second.can_read()) return;
                std::uint8_t bytes[16];
                const auto r = try_receive(socket, bytes, sizeof(bytes));
                if (r.state == ReceiveState::would_block || r.state == ReceiveState::interrupted) return;
                if (r.state != ReceiveState::progress || !it->second.append(bytes, r.count)) {
                    close(event.id);
                    return;
                }
                pump(event.id);
            });
        require(id.has_value(), "attach");
        sessions.try_emplace(*id, *id);
        timers.arm(*id, TimerHeap::Clock::now() + 1s);
        return *id;
    }
    void tick() {
        const int timeout = timers.timeout_ms(TimerHeap::Clock::now());
        const auto batch = loop.observe(timeout < 0 ? 20 : std::min(timeout, 20));
        require(batch.state != PollState::error, "poll");
        std::vector<Offload::Cont> done;
        pool.drain(done);
        for (auto& f : done) f();
        loop.dispatch(batch);
        std::vector<TimerHeap::Token> due;
        timers.expired(TimerHeap::Clock::now(), due);
        for (auto id : due) if (sessions.count(id)) {
            ++timed_out;
            close(id);
        }
    }
    void finish() {
        pool.shutdown();
        std::vector<Offload::Cont> done;
        pool.drain(done);
        for (auto& f : done) f();
    }
};
int main(int argc, char** argv) {
    Runtime runtime;
    if (!runtime.ready()) return 2;
    try {
        SessionLoop owner(epoll_argument(argc, argv));
        auto a = pair(), b = pair(), silent = pair();
        const auto first = owner.attach(std::move(a.receiver));
        owner.attach(std::move(b.receiver));
        const auto silent_id = owner.attach(std::move(silent.receiver));
        owner.timers.arm(silent_id, TimerHeap::Clock::now() + 30ms);
        Frame admission{50, {}, 2};
        admission.payload[0] = 1;
        Frame ready{17, {}, 1};
        ready.payload[0] = 1;
        for (auto* socket : {&a.sender, &b.sender}) for (const auto& f : {admission, ready, probe_frame(3, 9)}) {
            EncodedFrame e;
            require(encode_frame(f, e), "encode");
            require(send_bounded(*socket, e.bytes.data(), e.size, TimerHeap::Clock::now() + 1s).outcome == SendOutcome::complete, "send");
        }
        const auto stop = TimerHeap::Clock::now() + 2s;
        while (!owner.sessions.empty() && TimerHeap::Clock::now() < stop) owner.tick();
        owner.finish();
        owner.apply_auth(first, true);
        require(owner.games == 2 && owner.sessions.empty() && owner.ignored == 1 && owner.timed_out == 1, "final state");
        std::puts("state loop: admission -> worker auth -> both ready -> buffered game; silent timeout; late result ignored");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
