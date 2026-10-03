#pragma once
// widgets.h - pure immediate-mode widgets built on immediate_ui.h (C++17).
#include <cmath>
#include <cstddef>

#include "immediate_ui.h"

namespace study_ui {

// Checkbox reports the caller's current value and a press-triggered intent.
// toggle_requested is an intent for this frame, not the new value: the caller
// decides when and whether to apply it, so checked is never mutated here.
struct Checkbox {
    Button interaction;
    bool checked = false;
    bool toggle_requested = false;
};

// Rect, Input and enabled flow straight into button(). The returned checked is
// always the original value; toggle_requested mirrors the press edge only.
inline Checkbox checkbox(Rect bounds, const Input& input, bool checked,
                         bool enabled = true) noexcept {
    Checkbox box;
    box.checked = checked;
    box.interaction = button(bounds, input, enabled);
    box.toggle_requested = box.interaction.activated;
    return box;
}

// Selector exposes two square arrow buttons separated by a positive centre gap.
// direction is -1 (previous), +1 (next) or 0; it never wraps and stays 0 for an
// invalid index, count or geometry.
struct Selector {
    Button previous;
    Button next;
    bool previous_enabled = false;
    bool next_enabled = false;
    int direction = 0;
};

// Two square arrows of side h sit at the ends of bounds, so bounds must be a
// finite, non-degenerate rect wider than 2*h. Index and count are validated
// before any geometry, and any failure yields an all-default Selector.
inline Selector selector(Rect bounds, const Input& input, std::size_t index,
                         std::size_t count, bool enabled = true) noexcept {
    Selector s;
    if (count == 0 || index >= count) return s;
    if (!contains(bounds, Point{bounds.x, bounds.y})) return s;
    if (!std::isfinite(2.0 * bounds.h)) return s;
    if (!(bounds.w > 2.0 * bounds.h)) return s;
    const Rect left{bounds.x, bounds.y, bounds.h, bounds.h};
    const Rect right{bounds.x + bounds.w - bounds.h, bounds.y, bounds.h, bounds.h};
    if (!contains(left, Point{left.x, left.y})) return s;
    if (!contains(right, Point{right.x, right.y})) return s;
    if (left.x + left.w > right.x) return s;  // arrows must not overlap
    s.previous_enabled = enabled && index > 0;
    s.next_enabled = enabled && index < count - 1;
    s.previous = button(left, input, s.previous_enabled);
    s.next = button(right, input, s.next_enabled);
    if (s.previous.activated) s.direction = -1;
    else if (s.next.activated) s.direction = 1;
    else s.direction = 0;
    return s;
}

}  // namespace study_ui
