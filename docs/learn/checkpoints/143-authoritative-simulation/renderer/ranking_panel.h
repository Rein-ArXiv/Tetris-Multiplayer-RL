#pragma once
#include "client/ranking_panel.h"
#include "renderer/ranking_labels.h"
#include "renderer/menu_controls.h"
namespace study_ranking_render {
inline bool draw(const study_ranking_ui::View& view,study_ui::Input pointer,
                 const Labels& rows,const study_menu_render::Labels& labels,
                 study_image::ImageStore& images,study_image::Handle white,
                 study_rounded::RoundedQuad& rounded,study_image_quad::ImageQuad& text) {
    using namespace study_ranking_ui;using L=study_menu_render::Label;
    pointer.press.reset();
    const auto rectangle=[&](study_ui::Rect r,study_image_quad::Tint color) {
        study_rounded::Draw d;d.image.rect={float(r.x),float(r.y),float(r.w),float(r.h)};
        d.image.tint=color;d.radius=4;return images.draw(rounded,white,d);
    };
    const bool busy=view.status==Status::busy;
    if(!rectangle({90,12,218,222},{.04f,.07f,.12f,1}) || !labels.centered(L::ranking,text,200,35))return false;
    if(view.status==Status::ready && !view.rows.empty()) {if(!rows.draw(text))return false;}
    else {
        const auto label=busy?L::loading:view.status==Status::failed?L::load_failed:
                         view.status==Status::ready?L::empty:L::rank_idle;
        if(!labels.centered(label,text,200,100))return false;
    }
    return rectangle(refresh_bounds,study_menu_render::control_color(study_ui::button(refresh_bounds,pointer,!busy),true,!busy)) &&
        labels.centered(busy?L::loading:L::refresh,text,148,216) &&
        rectangle(back_bounds,study_menu_render::control_color(study_ui::button(back_bounds,pointer),false)) &&
        labels.centered(L::back,text,252,216);
}
} // namespace study_ranking_render
