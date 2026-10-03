#pragma once
// menu_model.h - pure focus/action model for the main menu (C++17).
#include <cmath>
#include <cstddef>

#include "widgets.h"

namespace study_menu {

enum class Focus { start, decorations, badge };
enum class Action { none, start, toggle_decorations, previous_badge, next_badge };

// Preferences owns presentation values. Focus is held separately by the caller;
// GPU handles and simulation state belong to their own owners.
class Preferences final {
public:
    static constexpr std::size_t badge_count = 2;

    bool decorations() const noexcept { return decorations_; }
    std::size_t badge() const noexcept { return badge_; }

    // Apply one intent exactly once. Returns true only when a stored value
    // actually changed; none, start, invalid actions and clamped endpoints are
    // reported as false. Limits are checked before ++/-- to avoid overflow.
    bool apply(Action action) noexcept {
        switch (action) {
            case Action::toggle_decorations:
                decorations_ = !decorations_;
                return true;
            case Action::previous_badge:
                if (badge_ == 0) return false;
                --badge_;
                return true;
            case Action::next_badge:
                if (badge_ >= badge_count - 1) return false;
                ++badge_;
                return true;
            case Action::none:
            case Action::start:
            default:
                return false;
        }
    }

private:
    bool decorations_ = true;
    std::size_t badge_ = 0;
};

// Fixed, non-overlapping logical controls shared with the renderer.
inline constexpr study_ui::Rect start_bounds{14, 64, 64, 28};
inline constexpr study_ui::Rect decorations_bounds{110, 84, 180, 24};
inline constexpr study_ui::Rect badge_bounds{110, 124, 180, 24};

// Per-frame press edges produced by the learning keyboard mapping.
struct Keys {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool confirm = false;
    bool cancelled = false; // Keyboard focus cancellation, independent of mouse availability.
};

struct Intent {
    Focus focus = Focus::start;
    Action action = Action::none;
};

namespace detail {

inline Focus normalize(Focus focus) noexcept {
    switch (focus) {
        case Focus::start:
        case Focus::decorations:
        case Focus::badge:
            return focus;
        default:
            return Focus::start;
    }
}

}  // namespace detail

// Pure per-frame evaluation; call prefs.apply(intent.action) exactly once
// afterwards and only when intent.action is not Action::none.
inline Intent evaluate(const Preferences& prefs, Focus current,
                       const study_ui::Input& input, Keys keys) noexcept {
    const Focus focus = detail::normalize(current);

    // Keyboard focus loss cancels this menu frame. Mouse unavailability alone
    // still permits keyboard operation (e.g. pointer outside the window).
    if (keys.cancelled) return Intent{focus, Action::none};

    // First-press routing: one press is offered to start, then the decorations
    // checkbox, then the badge selector, and the first widget containing it
    // consumes the frame. A press on a disabled arrow still consumes and simply
    // yields no action, so a later Space edge cannot reach another widget.
    if (study_ui::button(start_bounds, input, true).activated)
        return Intent{Focus::start, Action::start};

    const study_ui::Checkbox deco =
        study_ui::checkbox(decorations_bounds, input, prefs.decorations(), true);
    if (deco.toggle_requested)
        return Intent{Focus::decorations, Action::toggle_decorations};

    if (!input.cancelled && input.press && study_ui::contains(badge_bounds, *input.press)) {
        const auto selector = study_ui::selector(badge_bounds, input, prefs.badge(), Preferences::badge_count);
        const auto action = selector.direction < 0 ? Action::previous_badge
                          : selector.direction > 0 ? Action::next_badge : Action::none;
        // The center and disabled arrows consume the press too. They focus the
        // selector without letting a simultaneous keyboard confirm start play.
        return Intent{Focus::badge, action};
    }

    // Keyboard: navigation moves focus first, then the action is chosen for the
    // resulting focus. Up/down are exclusive, as are left/right on the badge;
    // mouse input above always wins over this path. Confirm is ignored on the
    // badge, so it never disables stepping, and endpoint steps are suppressed
    // by index so no intent is emitted that apply() would reject.
    Focus next_focus = focus;
    if (keys.up != keys.down) {
        int idx = static_cast<int>(next_focus);
        idx += keys.up ? -1 : 1;
        if (idx < 0) idx = 0;
        if (idx > 2) idx = 2;
        next_focus = static_cast<Focus>(idx);
    }

    Action action = Action::none;
    if (next_focus == Focus::start) {
        if (keys.confirm) action = Action::start;
    } else if (next_focus == Focus::decorations) {
        if (keys.confirm) action = Action::toggle_decorations;
    } else {
        if (keys.left != keys.right) {
            if (keys.left) {
                if (prefs.badge() > 0) action = Action::previous_badge;
            } else {
                if (prefs.badge() < Preferences::badge_count - 1)
                    action = Action::next_badge;
            }
        }
    }
    return Intent{next_focus, action};
}

}  // namespace study_menu
