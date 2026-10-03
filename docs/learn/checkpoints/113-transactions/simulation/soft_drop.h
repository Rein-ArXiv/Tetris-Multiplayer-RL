#pragma once
// simulation/soft_drop.h
// Deterministic CPU timing transition: it mutates only the supplied Counter.
// No chrono / GL / SDL / global state.

namespace study_soft_drop {

struct Counter {
    int remaining = 0;
    int period = 4;
};

enum class Result { idle, waiting, due, invalid };

// Validate period >= 1 and 0 <= remaining < period BEFORE mutating state.
// Invalid: return Result::invalid and change nothing.
// !held:                 remaining = 0, return idle.
// held, remaining > 0:   --remaining, return waiting.
// held, remaining == 0:  remaining = period - 1, return due.
//
// period == 4 held continuously fires due on held ticks 1, 5, 9, ...
// (tick 1: remaining 0 -> due, remaining := 3; ticks 2-4 waiting; tick 5: due).
// Release (!held) resets remaining to 0, so the next hold fires immediately.
// period == 1 fires due on every held tick.
// INT_MAX is a valid period and never overflows.
inline Result tick(bool held, Counter& c) noexcept {
    if (c.period < 1 || c.remaining < 0 || c.remaining >= c.period) {
        return Result::invalid;
    }
    if (!held) {
        c.remaining = 0;
        return Result::idle;
    }
    if (c.remaining > 0) {
        --c.remaining;
        return Result::waiting;
    }
    c.remaining = c.period - 1;
    return Result::due;
}

} // namespace study_soft_drop
