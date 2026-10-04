#pragma once
#include "client/account_panel.h"
#include "renderer/menu_controls.h"
namespace study_account_render {
inline study_menu_render::Label status_label(study_account_ui::Status status) {
    using S=study_account_ui::Status;using L=study_menu_render::Label;
    switch(status) {
    case S::idle:return L::idle;
    case S::busy:return L::busy;
    case S::online:return L::online;
    case S::offline_saved:return L::offline;
    case S::unsaved:return L::unsaved;
    case S::storage_blocked:return L::blocked;
    case S::key_rejected:return L::rejected;
    case S::creation_unconfirmed:return L::uncertain;
    default:return L::failed;
    }
}
// Read-only view and hover state. No worker call is allowed in drawing.
inline bool draw(const study_account_ui::View& view, study_ui::Input pointer,
                 const study_menu_render::Labels& labels, study_image::ImageStore& images,
                 study_image::Handle white, study_rounded::RoundedQuad& rounded,
                 study_image_quad::ImageQuad& text) {
    using namespace study_account_ui;using L=study_menu_render::Label;
    pointer.press.reset();
    const auto rectangle=[&](study_ui::Rect bounds,study_image_quad::Tint color) {
        study_rounded::Draw request;
        request.image.rect={float(bounds.x),float(bounds.y),float(bounds.w),float(bounds.h)};
        request.image.tint=color;request.radius=4;
        return images.draw(rounded,white,request);
    };
    const bool enabled=may_connect(view);
    return rectangle({90,12,218,222},{.04f,.07f,.12f,1}) &&
        labels.centered(L::account,text,200,45) &&
        labels.centered(status_label(view.status),text,200,80) &&
        (!view.has_unsaved_key || labels.centered(L::lose_key,text,200,108)) &&
        rectangle(connect_bounds,study_menu_render::control_color(
            study_ui::button(connect_bounds,pointer,enabled),true,enabled)) &&
        labels.centered(view.status==Status::busy ? L::busy :
            view.status==Status::creation_unconfirmed ? L::uncertain : L::connect,text,200,151) &&
        rectangle(back_bounds,study_menu_render::control_color(study_ui::button(back_bounds,pointer),false)) &&
        labels.centered(L::back,text,200,195);
}
} // namespace study_account_render
