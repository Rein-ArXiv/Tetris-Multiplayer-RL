#pragma once
#include "simulation/pending_horizontal.h"

namespace study_input {

struct Intent {
    int horizontal;
    bool clockwise;
    bool soft_drop = false;
    bool hard_drop = false;
};

class PendingControls {
public:
    // Capture one frame of raw input.
    //
    // left/right/up/spacePressed represent captured edge-like inputs.
    // Up and Space each use a bool latch: the next consume reports a request
    // once, even if more than one press was captured before that consume.
    //
    // downHeld is a *held-state* sample: it is NOT latched as an event, so the
    // latest capture always REPLACES the previous downHeld (use assignment, not
    // OR-latching). The stored level is reported by every subsequent consume().
    //
    // The 4th parameter defaults to false so existing three-argument capture
    // calls keep compiling and behave as "no soft drop held". The 5th parameter
    // defaults to false so existing calls behave as "no hard drop pressed".
    void capture(bool left, bool right, bool up, bool downHeld = false,
                 bool spacePressed = false) noexcept {
        horizontal_.capture(left, right);
        clockwise_ = clockwise_ || up;
        soft_drop_ = downHeld;
        // A bool latch is enough here: it coalesces any number of captured
        // hard-drop edges into a single pending request. It is NOT a queue and
        // gives no physical exact-once guarantee; two presses captured before
        // one consume() still yield one hard_drop.
        hard_drop_ = hard_drop_ || spacePressed;
    }

    // Build one simulation tick intent.
    //
    // Horizontal/rotation edges are consumed once (cleared here). hard_drop is
    // cleared exactly once here as well, after being latched by capture().
    // soft_drop is the most recent held sample and is re-reported on EVERY call,
    // so the timing gate can inspect it until a later capture replaces it.
    //
    // Note: because downHeld is a level and not an edge, a short Down tap that is
    // captured and then replaced by a later zero-tick frame before consume()
    // is intentionally NOT preserved. This differs from the Up edge, which
    // is latched and cannot be lost this way. The hard_drop latch behaves like
    // the Up edge, not like downHeld.
    Intent consume() noexcept {
        Intent result{horizontal_.consume(), clockwise_, soft_drop_, hard_drop_};
        clockwise_ = false;
        hard_drop_ = false;
        return result;
    }

private:
    PendingHorizontal horizontal_;
    bool clockwise_ = false;
    bool soft_drop_ = false;
    bool hard_drop_ = false;
};

} // namespace study_input
