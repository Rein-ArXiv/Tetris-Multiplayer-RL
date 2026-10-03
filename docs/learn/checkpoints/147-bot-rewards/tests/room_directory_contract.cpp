#include "net/room_directory.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <thread>
#include <vector>
using namespace study_net;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)
using Payload = std::unique_ptr<unsigned>;
using Directory = RoomDirectory<Payload, 4>;
struct NoAssign {
    unsigned value;
    explicit NoAssign(unsigned n) : value(n) {}
    NoAssign(NoAssign&&) noexcept = default;
    NoAssign& operator=(NoAssign&&) = delete;
};
int main() {
    for (unsigned position = 0; position < 5; ++position) {
        for (unsigned digit = 0; digit < 32; ++digit) {
            auto code = code_from_word(digit << (5 * position));
            RoomCode expected{'A','A','A','A','A'};
            expected[position] = kRoomCodeAlphabet[digit];
            CHECK(code == expected && parse_room_code(room_code_text(code)) == code);
        }
    }
    for (auto text : {"", "AAAA", "AAAAAA", "aaaaa", "AAAA0", "AAAA1", "AAAAI", "AAAAO", " AAAA"})
        CHECK(!parse_room_code(text));
    CHECK(room_code_text(code_from_word(0)) == "AAAAA");
    CHECK(room_code_text(code_from_word(UINT32_MAX)) == "99999");
    auto zero = []() noexcept -> std::optional<std::uint32_t> { return 0; };
    {
        RoomDirectory<NoAssign,1> q;
        NoAssign x{9}; auto made = q.create(1,x,zero);
        CHECK(made.handle); auto out = q.take(*made.handle); CHECK(out && out->value.value == 9);
    }
    {
        Directory q; auto value = std::make_unique<unsigned>(9);
        auto a = q.create(1,value,zero); CHECK(a.handle && !value);
        auto taken = q.take(*a.handle); CHECK(taken);
        value = std::move(taken->value);
        auto b = q.create(1,value,zero); CHECK(b.handle && b.handle->generation != a.handle->generation);
        CHECK(!q.take(*a.handle)); // Same visible code and owner; old generation loses.
        auto wrong_owner = *b.handle; wrong_owner.owner = 2; CHECK(!q.take(wrong_owner));
        unsigned calls = 0; value = std::make_unique<unsigned>(8); auto* before = value.get();
        auto collision = q.create(2,value,[&]() noexcept -> std::optional<uint32_t> { ++calls; return 0; });
        CHECK(collision.status == RoomCreate::collision_limit && calls == 32 && value.get() == before);
        calls = 0;
        auto failed = q.create(2,value,[&]() noexcept -> std::optional<uint32_t> { ++calls; return {}; });
        CHECK(failed.status == RoomCreate::source_failed && calls == 1 && value.get() == before);
        auto c = q.create(2,value,[]() noexcept -> std::optional<uint32_t> { return 1; });
        CHECK(c.handle && c.handle->generation == b.handle->generation + 1);
        q.close(); q.close();
        value = std::make_unique<unsigned>(7); calls = 0;
        auto closed = q.create(0,value,[&]() noexcept -> std::optional<uint32_t> { ++calls; return 9; });
        CHECK(closed.status == RoomCreate::closed && calls == 0 && *value == 7 && q.size() == 0);
        CHECK(!q.take(*b.handle));
    }
    {
        Directory q(UINT64_MAX); auto value = std::make_unique<unsigned>(1);
        auto last = q.create(1,value,zero); CHECK(last.handle && last.handle->generation == UINT64_MAX);
        auto taken = q.take(*last.handle); CHECK(taken); value = std::move(taken->value);
        unsigned calls = 0;
        auto exhausted = q.create(1,value,[&]() noexcept -> std::optional<uint32_t> { ++calls; return 1; });
        CHECK(exhausted.status == RoomCreate::generation_exhausted && calls == 0 && value);
    }
    unsigned rng = 103;
    for (unsigned trial = 0; trial < 64; ++trial) {
        Directory q; std::map<RoomCode,RoomHandle> model;
        std::vector<RoomHandle> history; uint64_t next_generation = 1;
        for (unsigned step = 0; step < 256; ++step) {
            rng = rng * 1664525u + 1013904223u;
            const unsigned owner = (rng >> 8) % 7, word = (rng >> 12) % 8, op = (rng >> 20) % 3;
            const auto code = code_from_word(word);
            if (op == 0) {
                const bool duplicate = std::any_of(model.begin(),model.end(),[&](const auto& p) { return p.second.owner == owner; });
                const RoomCreate want = owner == 0 ? RoomCreate::invalid_owner : duplicate ? RoomCreate::duplicate_owner :
                    model.size() == 4 ? RoomCreate::full : model.count(code) ? RoomCreate::collision_limit : RoomCreate::created;
                auto value = std::make_unique<unsigned>(owner + 100); auto* before = value.get(); unsigned calls = 0;
                auto made = q.create(owner,value,[&]() noexcept -> std::optional<uint32_t> { ++calls; return word; });
                CHECK(made.status == want && bool(made.handle) == (want == RoomCreate::created));
                if (made.handle) {
                    CHECK(!value && calls == 1 && made.handle->generation == next_generation++);
                    model.emplace(code,*made.handle); history.push_back(*made.handle);
                } else CHECK(value.get() == before && calls == (want == RoomCreate::collision_limit ? 32u : 0u));
            } else if (op == 1 && !history.empty()) {
                auto h = history[(rng >> 4) % history.size()]; auto it = model.find(h.code);
                const bool exists = it != model.end() && it->second.generation == h.generation;
                auto out = q.take(h); CHECK(bool(out) == exists);
                if (out) { CHECK(*out->value == h.owner + 100); model.erase(it); }
            } else {
                auto found = q.find(code); auto it = model.find(code); CHECK(bool(found) == (it != model.end()));
                if (found) CHECK(found->generation == it->second.generation && found->owner == it->second.owner);
            }
            CHECK(q.size() == model.size());
        }
    }
    for (unsigned trial = 0; trial < 64; ++trial) {
        Directory q; std::atomic<bool> start{false}; std::array<RoomCreated,2> results;
        auto work = [&](unsigned i) {
            auto value = std::make_unique<unsigned>(i);
            while (!start.load()) std::this_thread::yield();
            results[i] = q.create(i + 1,value,zero);
        };
        std::thread a(work,0),b(work,1); start = true; a.join(); b.join();
        CHECK(bool(results[0].handle) != bool(results[1].handle));
        CHECK(q.size() == 1);
        const auto& failed = results[0].handle ? results[1] : results[0];
        CHECK(failed.status == RoomCreate::collision_limit);
    }
    std::puts("room directory: 160 digit checks, 16384 reference-model steps, 64 concurrent collisions, stale generation, failure ownership and exhaustion");
}
