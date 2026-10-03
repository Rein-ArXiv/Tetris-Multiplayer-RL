#pragma once
#include "client/ranking_controller.h"
#include "client/widgets.h"
namespace study_ranking_ui {
enum class Action {none,refresh,back};
inline constexpr study_ui::Rect refresh_bounds{98,196,99,30};
inline constexpr study_ui::Rect back_bounds{203,196,99,30};
inline Action evaluate(const View& view,const study_ui::Input& input,bool confirm,bool back,bool cancelled) {
    if(cancelled)return Action::none;
    if(back)return Action::back;
    if(!input.cancelled && input.press) {
        if(study_ui::contains(back_bounds,*input.press))return Action::back;
        if(study_ui::contains(refresh_bounds,*input.press) && view.status!=Status::busy)return Action::refresh;
        return Action::none;
    }
    return confirm && view.status!=Status::busy ? Action::refresh : Action::none;
}
} // namespace study_ranking_ui
