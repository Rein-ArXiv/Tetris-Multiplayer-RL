// Standalone test for tick_gate.hpp. No assert; returns nonzero on failure.
//   c++ -std=c++17 -Wall -Wextra -pedantic tick_gate_test.cpp -o tick_gate_test
#include "../bot/pacing.h"

#include <cstdio>
#include <stdexcept>

using bot::Pacing;
using bot::TickGate;

static int g_failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

// Runs exactly n begin_tick() calls and returns the zero-based index of the
// first tick whose begin_tick() returned true, or -1.
static int first_open(TickGate& gate, int n) {
    for (int i = 0; i < n; ++i) {
        if (gate.begin_tick()) return i;
    }
    return -1;
}

int main() {
    // Range clamping helpers.
    {
        Pacing lo = Pacing::clamped(0, -5, 9999);
        check(lo.interval == 1 && lo.think == 0 && lo.minimum == 600, "clamp low/high");
        check(lo.valid(), "clamped result valid");
        Pacing hi = Pacing::clamped(100, 200, 0);
        check(hi.interval == 30 && hi.think == 180 && hi.minimum == 1, "clamp high/low");
    }

    // Timeline: think=2 interval=3 minimum=8.
    {
        Pacing p;
        p.interval = 3;
        p.think = 2;
        p.minimum = 8;
        check(p.valid(), "timeline pacing valid");

        TickGate gate(p);
        check(first_open(gate, 3) == 2, "first operation at zero-based tick 2");
        check(!gate.drop_ready(), "not drop ready at tick 2");
        gate.defer(); // cooldown = interval-1 = 2

        int second = -1;
        for (int i = 3; i < 6; ++i) {
            if (gate.begin_tick()) { second = i; break; }
        }
        check(second == 5, "second operation at tick 5");
        gate.defer();

        int drop = -1;
        for (int i = 6; i < 10; ++i) {
            const bool open = gate.begin_tick();
            if (open && gate.drop_ready()) { drop = i; break; }
        }
        check(drop == 8, "drop at earliest tick 8 (cooldown excludes 7)");
    }

    // Pure drop: think=0 interval=1 minimum=3 is allowed at tick 2.
    {
        Pacing p;
        p.interval = 1;
        p.think = 0;
        p.minimum = 3;
        check(p.valid(), "pure-drop pacing valid");

        TickGate gate(p);
        int drop = -1;
        for (int i = 0; i < 6; ++i) {
            const bool open = gate.begin_tick();
            if (open && gate.drop_ready()) { drop = i; break; }
        }
        check(drop == 2, "pure drop allowed at tick 2");
    }

    // Defaults: first pure drop at tick 59.
    {
        TickGate gate;
        check(gate.config().interval == 6 && gate.config().think == 18 &&
                  gate.config().minimum == 60,
              "default pacing values");

        int drop = -1;
        for (int i = 0; i < 100; ++i) {
            const bool open = gate.begin_tick();
            if (open && gate.drop_ready()) { drop = i; break; }
        }
        check(drop == 59, "default first pure drop at tick 59");
    }

    // Failed reset preserves state.
    {
        Pacing p;
        p.interval = 4;
        p.think = 3;
        p.minimum = 10;
        TickGate gate(p);
        for (int i = 0; i < 4; ++i) gate.begin_tick();
        gate.defer(); // cooldown = 3

        const Pacing before = gate.config();
        const int age_before = gate.age_for_gates();
        const int cool_before = gate.cooldown();

        bool threw = false;
        try {
            Pacing bad;
            bad.interval = 0; // below interval_min
            gate.reset(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "invalid reset throws");
        check(gate.config().interval == before.interval &&
                  gate.config().think == before.think &&
                  gate.config().minimum == before.minimum,
              "config preserved after failed reset");
        check(gate.age_for_gates() == age_before, "age preserved after failed reset");
        check(gate.cooldown() == cool_before, "cooldown preserved after failed reset");

        // A valid reset does restart the piece.
        gate.reset(Pacing{});
        check(gate.age_for_gates() == 0 && gate.cooldown() == 0, "valid reset restarts");
    }

    // Counters stay saturated and bounded after many calls.
    {
        Pacing p;
        p.interval = 30;
        p.think = 180;
        p.minimum = 600;
        check(p.valid(), "max pacing valid");
        TickGate gate(p);
        for (int i = 0; i < 10000; ++i) {
            gate.begin_tick();
            if (i % 7 == 0) gate.defer();
        }
        check(gate.age_for_gates() <= 600, "age saturated bounded");
        check(gate.age_for_gates() >= 0, "age nonnegative");
        check(gate.cooldown() >= 0 && gate.cooldown() <= 29, "cooldown bounded");
    }

    if (g_failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
