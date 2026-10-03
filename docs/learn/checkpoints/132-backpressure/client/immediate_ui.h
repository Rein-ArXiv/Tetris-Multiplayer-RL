#pragma once
// immediate_ui.h - pure immediate-mode hit testing over letterboxed input (C++17).
#include <cmath>
#include <optional>

#include "renderer/letterbox.h"
#include "platform/pointer_edges.h"

namespace study_ui {

using Point = study_letterbox::Point;

struct Rect { double x = 0.0, y = 0.0, w = 0.0, h = 0.0; };

struct Input {
    std::optional<study_letterbox::Point> position;
    std::optional<study_letterbox::Point> press;
    bool down = false;
    bool cancelled = false;
};

struct Button {
    bool hovered = false;
    bool held = false;
    bool activated = false;
};

// Half-open hit test in logical units. Reject degenerate or non-finite rects
// and non-finite points; no clamping, so a collapsed interval stays empty.
inline bool contains(Rect r, Point p) noexcept {
    if (!std::isfinite(r.x) || !std::isfinite(r.y) ||
        !std::isfinite(r.w) || !std::isfinite(r.h))
        return false;
    if (!(r.w > 0.0) || !(r.h > 0.0)) return false;
    const double rx = r.x + r.w;
    const double ry = r.y + r.h;
    if (!std::isfinite(rx) || !std::isfinite(ry)) return false;
    if (!(rx > r.x) || !(ry > r.y)) return false;  // collapsed by rounding
    if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
    return r.x <= p.x && p.x < rx && r.y <= p.y && p.y < ry;
}

// Convert one frame of native window input into logical input. Position and
// press are mapped independently, so a press that lands in the viewport still
// activates even when the current position is absent or outside it.
// layout, when present, must be an unchanged make_layout result.
inline Input map_pointer(const std::optional<study_letterbox::Layout>& layout,
                         const study_pointer::Frame& frame) noexcept {
    Input input;
    if (!layout || frame.cancelled) {
        input.cancelled = frame.cancelled;
        return input;
    }
    if (frame.position) {
        const study_letterbox::Point p{static_cast<double>(frame.position->x),
                                       static_cast<double>(frame.position->y)};
        input.position = study_letterbox::window_to_logical(*layout, p);
    }
    if (frame.press) {
        const study_letterbox::Point p{static_cast<double>(frame.press->x),
                                       static_cast<double>(frame.press->y)};
        input.press = study_letterbox::window_to_logical(*layout, p);
    }
    input.down = frame.down;
    input.cancelled = frame.cancelled;
    return input;
}

// Evaluate a button from already-mapped input. Press-triggered first-edge policy
// with no retained state or capture. The caller owns dispatch, so overlapping
// widgets may activate together; build non-overlapping layouts to avoid that.
inline Button button(Rect bounds, const Input& input, bool enabled = true) noexcept {
    if (!enabled || input.cancelled) return Button{};
    Button b;
    b.hovered = input.position && contains(bounds, *input.position);
    b.held = b.hovered && input.down;
    b.activated = input.press && contains(bounds, *input.press);
    return b;
}

}  // namespace study_ui
