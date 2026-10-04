#pragma once

#include "simulation/input_mask.h"

namespace study_input {

// Coalesce captured input into a byte mask until a tick consumes it.
//
// Edges (left/right/up/space): a bool-style OR latch that coalesces any number
// of presses captured before one consume() into a single pending request.
//
// downHeld is a *held-state* sample, not an edge: each capture REPLACES the
// previous sample (assignment, not OR). The stored level is reported by every
// subsequent consume.
//
// Frame/tick boundary: capture() runs once per render frame; consume()
// runs once per simulation tick and drains the accumulated mask. clear()
// discards everything, including the latest held sample.
class PendingControls {
public:
    // Discard unconsumed edges and the latest held-state sample.
    void clear() noexcept { *this = PendingControls{}; }

    // Capture one frame of raw input.
    void capture(bool left_pressed, bool right_pressed, bool up,
                 bool downHeld = false, bool spacePressed = false) noexcept {
        Mask edges = 0;
        if (left_pressed) {
            edges |= left;
        }
        if (right_pressed) {
            edges |= right;
        }
        if (up) {
            edges |= rotate;
        }
        if (spacePressed) {
            edges |= drop;
        }
        pending_ |= edges;
        // Held-state sample: replace, do not latch.
        soft_drop_ = downHeld;
    }

    // Build one simulation tick intent from the drained mask.
    Intent consume() noexcept {
        // Only allowed bits are ever stored, so decode() cannot fail here.
        return *decode(consume_mask());
    }

    // Drain the pending mask and fold in the current held-state sample.
    //
    // Edges are consumed once (cleared here). soft_drop_ is a level and is
    // re-reported on EVERY call until a later capture replaces it. A raw
    // left+right pair is preserved in the raw mask; decode() normalizes it to
    // a neutral horizontal.
    Mask consume_mask() noexcept {
        Mask mask = pending_;
        pending_ = 0;
        if (soft_drop_) {
            mask |= down;
        }
        return mask;
    }

private:
    Mask pending_ = 0;
    bool soft_drop_ = false;
};

} // namespace study_input
