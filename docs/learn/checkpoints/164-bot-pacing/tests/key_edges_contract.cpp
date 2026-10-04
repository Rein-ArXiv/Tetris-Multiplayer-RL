// tests/key_edges_contract.cpp
#include "core/key_edges.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>

// CHECK must stay active in Release builds (NDEBUG defined).
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",            \
                         __FILE__, __LINE__, #cond);                    \
            std::exit(1);                                               \
        }                                                               \
    } while (0)

namespace {

using Edges = input_detail::KeyEdges<2>;
constexpr std::size_t kCount = 2;

// Events fed to the class under test.
enum Event : int { kDown = 0, kUp, kRepeat, kCancel, kBegin, kEventCount };

// History oracle: derive states from the last relevant events, not latches.
struct RefModel {
    std::array<Event, 8> history{};
    std::size_t size = 0;
    void append(Event event) { history[size++] = event; }
    bool held_at(std::size_t end) const {
        while (end) {
            const auto event = history[--end];
            if (event == kDown) return true;
            if (event == kUp || event == kCancel) return false;
        }
        return false;
    }
    std::size_t boundary(bool press) const {
        for (auto end = size; end; --end)
            if (history[end - 1] == kBegin || (press && history[end - 1] == kCancel))
                return end;
        return 0;
    }
    bool edge(bool press) const {
        for (auto i = boundary(press); i < size; ++i) {
            if (press && history[i] == kDown && !held_at(i)) return true;
            if (!press && (history[i] == kUp || history[i] == kCancel) && held_at(i)) return true;
        }
        return false;
    }
    bool cancelled() const {
        for (auto i = boundary(false); i < size; ++i)
            if (history[i] == kCancel) return true;
        return false;
    }
};

void apply(Edges& e, RefModel& r, Event ev, std::size_t i) {
    r.append(ev);
    switch (ev) {
        case kDown: e.set(i, true); break;
        case kUp: e.set(i, false); break;
        case kRepeat: e.set(i, true, true); break;
        case kCancel: e.cancel(); break;
        case kBegin: e.begin_frame(); break;
        default: CHECK(false);
    }
}
void check_match(const Edges& e, const RefModel& r, std::size_t i) {
    CHECK(e.down(i) == r.held_at(r.size));
    CHECK(e.pressed(i) == r.edge(true));
    CHECK(e.released(i) == r.edge(false));
    CHECK(e.cancelled() == r.cancelled());
}

// Exhaustive: every event sequence of length 0..7 on one key (max 5^7).
void exhaustive_sequences() {
    for (std::size_t len = 0; len <= 7; ++len) {
        std::size_t total = 1;
        for (std::size_t i = 0; i < len; ++i) total *= kEventCount;
        for (std::size_t code = 0; code < total; ++code) {
            Edges e;
            RefModel r;
            std::size_t c = code;
            for (std::size_t i = 0; i < len; ++i) {
                const Event ev = static_cast<Event>(c % kEventCount);
                c /= kEventCount;
                apply(e, r, ev, 0);
                check_match(e, r, 0);
            }
        }
    }
}

void test_independence() {
    Edges e;
    e.set(0, true, false);
    CHECK(e.down(0));
    CHECK(!e.down(1));
    CHECK(e.pressed(0));
    CHECK(!e.pressed(1));
    e.set(1, true, false);
    e.set(0, false, false);
    CHECK(!e.down(0));
    CHECK(e.down(1));
    CHECK(e.released(0));
    CHECK(!e.released(1));
}

void test_invalid_indices() {
    Edges e;
    const std::size_t bad[] = {kCount, std::numeric_limits<std::size_t>::max()};
    for (std::size_t i : bad) {
        e.set(i, true, false);
        CHECK(!e.down(i));
        CHECK(!e.pressed(i));
        CHECK(!e.released(i));
    }
    e.set(0, true, false);
    CHECK(e.down(0));  // a bad set() must not disturb valid keys
}

void test_getters_nonconsuming() {
    Edges e;
    e.set(0, true, false);
    CHECK(e.pressed(0));
    CHECK(e.pressed(0));
    CHECK(e.down(0));
    CHECK(e.down(0));
    e.begin_frame();
    CHECK(!e.pressed(0));
    CHECK(e.down(0));
}

void test_frame_boundary() {
    Edges e;
    e.set(0, true, false);
    e.set(0, false, false);
    CHECK(e.pressed(0) && e.released(0));
    e.begin_frame();
    CHECK(!e.down(0));
    CHECK(!e.pressed(0));
    CHECK(!e.released(0));
    e.set(0, true, false);  // held must survive a frame boundary
    e.begin_frame();
    CHECK(e.down(0));
    CHECK(!e.pressed(0));
}

void test_same_pump_tap() {
    Edges e;
    e.set(0, true, false);
    e.set(0, false, false);
    CHECK(e.pressed(0));
    CHECK(e.released(0));
    CHECK(!e.down(0));
}

void test_multi_tap_merge() {
    Edges e;
    e.set(0, true, false);
    e.set(0, false, false);
    e.set(0, true, false);
    e.set(0, false, false);
    CHECK(e.pressed(0));   // bool latches merge
    CHECK(e.released(0));
    CHECK(!e.down(0));
    Edges g;
    g.set(0, true, false);
    g.set(0, false, false);
    g.set(0, true, false);
    CHECK(g.pressed(0));
    CHECK(g.released(0));
    CHECK(g.down(0));
}

void test_autorepeat_suppressed() {
    Edges e;
    e.set(0, true, false);
    e.begin_frame();
    CHECK(e.down(0));
    CHECK(!e.pressed(0));
    e.set(0, true, true);  // repeat while held: no new press
    CHECK(e.down(0));
    CHECK(!e.pressed(0));
}

void test_repeat_after_focus_loss() {
    Edges e;
    e.set(0, true, false);
    e.cancel();            // focus loss
    CHECK(!e.down(0));
    CHECK(!e.pressed(0));
    e.set(0, true, true);  // repeat must not synthesize a press
    CHECK(!e.down(0));
    CHECK(!e.pressed(0));
}

void test_fresh_press_after_cancel() {
    Edges e;
    e.set(0, true, false);
    CHECK(e.pressed(0));
    e.cancel();
    CHECK(!e.pressed(0));  // same-frame press erased
    CHECK(!e.down(0));
    e.set(0, true, false); // new press in the same frame is accepted
    CHECK(e.pressed(0));
    CHECK(e.down(0));
}

void test_cancel_flag_and_reset() {
    Edges e;
    e.set(0, true, false);
    CHECK(!e.cancelled());
    e.cancel();
    CHECK(e.cancelled());
    CHECK(e.released(0));
    CHECK(!e.down(0));
    e.begin_frame();
    CHECK(!e.cancelled());
    e.set(0, true, false);
    e.set(1, true, false);
    e.reset();
    CHECK(!e.down(0) && !e.down(1));
    CHECK(!e.pressed(0) && !e.pressed(1));
    CHECK(!e.released(0) && !e.released(1));
    CHECK(!e.cancelled());
}

}  // namespace

int main() {
    exhaustive_sequences();
    test_independence();
    test_invalid_indices();
    test_getters_nonconsuming();
    test_frame_boundary();
    test_same_pump_tap();
    test_multi_tap_merge();
    test_autorepeat_suppressed();
    test_repeat_after_focus_loss();
    test_fresh_press_after_cancel();
    test_cancel_flag_and_reset();
    std::puts("key_edges contract: all checks passed");
    return 0;
}
