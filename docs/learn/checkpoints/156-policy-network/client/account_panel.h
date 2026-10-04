#pragma once
#include "client/account_view.h"
#include "client/widgets.h"
namespace study_account_ui {
enum class Action { none, connect, back };
inline constexpr study_ui::Rect connect_bounds{110,132,180,28};
inline constexpr study_ui::Rect back_bounds{110,176,180,28};
inline Action evaluate(const View& view, const study_ui::Input& input,
                       bool confirm, bool back, bool cancelled) {
    if (cancelled) return Action::none;
    if (back) return Action::back;
    if (!input.cancelled && input.press) {
        if (study_ui::contains(back_bounds,*input.press)) return Action::back;
        if (study_ui::contains(connect_bounds,*input.press))
            return may_connect(view) ? Action::connect : Action::none;
        return Action::none; // A pointer press consumes this frame's confirm edge.
    }
    return confirm && may_connect(view) ? Action::connect : Action::none;
}
} // namespace study_account_ui
