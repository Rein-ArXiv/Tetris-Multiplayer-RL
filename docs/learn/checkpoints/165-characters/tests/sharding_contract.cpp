#include "net/sharded_relay.h"
#include "net/transfer_inbox.h"
#include "tests/reactor_fixture.h"
#include <atomic>
#include <thread>
#include <cstdio>
using namespace study_net;
using namespace study_fixture;
struct FakeReactor final : StudyReactor {
    Registrations registrations;
    int calls = 0, fail_at = 0, removes = 0, remove_fail_at = 0;
    std::optional<Registration> watch(const Socket& s, unsigned i) override {
        if (++calls == fail_at) return {};
        return registrations.add(s.native(), i);
    }
    bool change(Registration id, unsigned i) override { return registrations.modify(id, i); }
    bool unwatch(Registration id) override {
        if (++removes == remove_fail_at) return false;
        return registrations.remove(id);
    }
    bool current(Registration id) const override { return registrations.find(id) != nullptr; }
    ReadyBatch poll(int) override { return {}; }
    bool wake() noexcept override { return true; }
    std::size_t count() const {
        std::size_t n = 0; for (const auto& item : registrations.entries()) if (item) ++n; return n;
    }
};
struct Payload { int number; int checksum; };
void mailbox_contract() {
    TransferInbox<Payload, 2> inbox;
    auto a = std::make_unique<Payload>(Payload{7, 21});
    auto b = std::make_unique<Payload>(Payload{8, 24});
    auto c = std::make_unique<Payload>(Payload{9, 27});
    auto* identity = a.get();
    require(inbox.try_push(a) && !a && inbox.try_push(b), "publication moves ownership");
    require(!inbox.try_push(c) && c && c->number == 9, "full retains ownership");
    auto batch = inbox.take();
    require(batch[0].get() == identity && batch[1]->number == 8, "address and FIFO");
    require(inbox.try_push(c), "capacity returned");
    auto final = inbox.close_and_take();
    require(final[0]->number == 9 && !final[1], "close drains accepted item");
    c = std::make_unique<Payload>(Payload{10, 30});
    require(!inbox.try_push(c) && c, "closed forever");
    require(!inbox.close_and_take()[0], "idempotent close");

    TransferInbox<Payload, 8> concurrent;
    std::atomic<int> done{0};
    std::array<std::thread, 4> producers;
    for (int p = 0; p < 4; ++p) producers[p] = std::thread([&, p] {
        for (int i = 0; i < 500; ++i) {
            const int n = p * 500 + i;
            auto value = std::make_unique<Payload>(Payload{n, n * 3});
            while (!concurrent.try_push(value)) std::this_thread::yield();
        }
        ++done;
    });
    std::array<bool, 2000> seen{};
    int received = 0;
    while (received < 2000) {
        auto next = concurrent.take();
        for (auto& value : next) if (value) {
            require(value->number >= 0 && value->number < 2000, "published number");
            require(value->checksum == value->number * 3 && !seen[value->number], "published data once");
            seen[value->number] = true; ++received;
        }
        std::this_thread::yield();
    }
    for (auto& producer : producers) producer.join();
    require(done == 4, "joined producers");
    require(!concurrent.close_and_take()[0], "all deliveries collected");
    TransferInbox<Payload, 8> closing;
    std::atomic<int> accepted{0};
    std::array<std::thread, 4> racers;
    for (auto& racer : racers) racer = std::thread([&] {
        for (int n = 0; n < 500; ++n) {
            auto value = std::make_unique<Payload>(Payload{n, n * 3});
            if (closing.try_push(value)) ++accepted;
            else require(bool(value), "rejection retains ownership during close race");
        }
    });
    auto last = closing.close_and_take();
    for (auto& racer : racers) racer.join();
    int delivered = 0; for (auto& value : last) if (value) ++delivered;
    require(delivered == accepted && !closing.take()[0], "close linearizes with publication");
}
int main() {
    Runtime runtime;
    if (!runtime.ready()) return 2;
    try {
        mailbox_contract();
        std::atomic<std::size_t> total{0};
        auto a = pair(), b = pair();
        auto match = std::make_unique<RelayMatch>(17, std::move(a.receiver), std::move(b.receiver), total);
        auto* identity = match.get();
        std::array<std::uint8_t, 1024> data{};
        require(match->queues[1].append(data.data(), data.size()) == BufferResult::stored, "seed pending output");
        const auto deadline = RelayMatch::Clock::now() + std::chrono::seconds(30);
        match->deadlines[1] = deadline;
        auto raw = std::make_unique<FakeReactor>(); auto* observed = raw.get();
        MatchLoop front(std::move(raw));
        require(front.attach(match) && !match && observed->count() == 2, "front attachment");
        const auto old = observed->registrations.entries()[0]->id;
        ReadyBatch stale; stale.events.push_back({old, true, true, false});
        auto transfer = front.detach(17);
        require(transfer.get() == identity && !front.contains(17) && observed->count() == 0, "detach graph");
        front.dispatch(stale);
        require(total == 1024 && transfer->queues[1].paused(), "old batch ignored");
        require(transfer->deadlines[1] == deadline, "deadline preserved");
        for (int failure : {1, 2}) {
            auto backend = std::make_unique<FakeReactor>(); auto* inspect = backend.get(); inspect->fail_at = failure;
            MatchLoop target(std::move(backend));
            require(!target.attach(transfer) && transfer.get() == identity, "registration failure retains payload");
            require(inspect->count() == 0 && target.size() == 0 && total == 1024, "registration rollback");
        }
        auto backend = std::make_unique<FakeReactor>(); auto* inspect = backend.get();
        MatchLoop target(std::move(backend));
        require(target.attach(transfer) && !transfer, "new owner");
        require(inspect->registrations.entries()[0]->interest == 0, "paused Read retained");
        require(inspect->registrations.entries()[1]->interest == (Read | Write), "pending Write retained");
        transfer = target.detach(17);
        transfer->deadlines[1] = RelayMatch::Clock::now() - std::chrono::seconds(1);
        require(target.attach(transfer), "expired transfer accepted for owner cleanup");
        target.tick(0);
        require(target.size() == 0 && total == 0 && inspect->count() == 0, "transfer does not renew expiry");
        for (int failure : {1, 2}) {
            auto c = pair(), d = pair();
            auto m = std::make_unique<RelayMatch>(21, std::move(c.receiver), std::move(d.receiver), total);
            require(m->queues[0].append(data.data(), data.size()) == BufferResult::stored, "detach failure budget");
            auto backend2 = std::make_unique<FakeReactor>(); backend2->remove_fail_at = failure;
            MatchLoop broken(std::move(backend2));
            require(broken.attach(m), "attach before detach fault");
            require(!broken.detach(21) && broken.size() == 0 && total == 0, "detach failure invalidates backend before cleanup");
        }
        std::puts("bounded inbox publication/FIFO; 2000 concurrent items; pair ownership, stale batch, masks, rollback, deadlines passed");
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
