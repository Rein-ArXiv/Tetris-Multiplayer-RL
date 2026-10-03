#pragma once
// menu_model.h - pure focus/action model for the main menu (C++17).
#include <cmath>
#include <cstddef>

#include "widgets.h"
#include "content/characters.h"

namespace study_menu {

enum class Focus { start, decorations, badge, account };
enum class Action { none, start, toggle_decorations, previous_badge, next_badge, account };

// Preferences owns presentation values. Focus is held separately by the caller;
// GPU handles and simulation state belong to their own owners.
class Preferences final {
public:
    static constexpr std::size_t badge_count = study_characters::characters.size();

    bool decorations() const noexcept { return decorations_; }
    bool set_decorations(bool value) noexcept {
        const bool changed = decorations_ != value;
        decorations_ = value;
        return changed;
    }
    std::size_t badge() const noexcept { return *study_characters::find_index(selected_id_); }
    std::string_view character_id() const noexcept { return selected_id_; }
    // Accept only known IDs and retain the catalog entry's static view, never the caller's
    // potentially temporary string. True means accepted, including the current ID.
    bool select_character(std::string_view id) noexcept {
        const auto* entry=study_characters::find(id);
        if(!entry) return false;
        selected_id_=entry->id;
        return true;
    }

    // Apply one intent exactly once. Returns true only when a stored value
    // actually changed; none, start, invalid actions and clamped endpoints are
    // reported as false. Limits are checked before ++/-- to avoid overflow.
    bool apply(Action action) noexcept {
        switch (action) {
            case Action::toggle_decorations:
                decorations_ = !decorations_;
                return true;
            case Action::previous_badge: {
                const auto index=badge();
                if (index == 0) return false;
                selected_id_=study_characters::characters[index-1].id;
                return true;
            }
            case Action::next_badge: {
                const auto index=badge();
                if (index >= badge_count - 1) return false;
                selected_id_=study_characters::characters[index+1].id;
                return true;
            }
            case Action::none:
            case Action::start:
            case Action::account:
            default:
                return false;
        }
    }

private:
    bool decorations_ = true;
    std::string_view selected_id_ = study_characters::default_id;
};

// Fixed, non-overlapping logical controls shared with the renderer.
inline constexpr study_ui::Rect account_bounds{14,104,64,28};
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
        case Focus::account:
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

    if (study_ui::button(account_bounds, input, true).activated)
        return Intent{Focus::account, Action::account};

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
        if (idx > 3) idx = 3;
        next_focus = static_cast<Focus>(idx);
    }

    Action action = Action::none;
    if (next_focus == Focus::start) {
        if (keys.confirm) action = Action::start;
    } else if (next_focus == Focus::decorations) {
        if (keys.confirm) action = Action::toggle_decorations;
    } else if (next_focus == Focus::account) {
        if (keys.confirm) action = Action::account;
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
