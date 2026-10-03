#pragma once
#include "client/menu_model.h"
#include "renderer/menu_labels.h"
#include "renderer/image_store.h"
#include "renderer/rounded_quad.h"

namespace study_menu_render {
inline study_image_quad::Tint control_color(const study_ui::Button& button, bool focused,
                                           bool enabled=true) noexcept {
    if (!enabled) return {.07f,.09f,.12f,1};
    if (button.held) return {.12f,.23f,.40f,1};
    if (button.hovered) return {.24f,.38f,.60f,1};
    if (focused) return {.30f,.25f,.09f,1};
    return {.15f,.22f,.35f,1};
}
// Drawing consumes the already-applied values. It never applies an intent or
// invokes Application. A second render pass therefore cannot toggle settings.
inline bool controls(const study_menu::Preferences& preferences, study_menu::Focus focus,
                     study_ui::Input pointer, const Labels& labels,
                     study_image::ImageStore& images, study_image::Handle white,
                     study_rounded::RoundedQuad& rounded, study_image_quad::ImageQuad& text) {
    pointer.press.reset();
    const auto rectangle=[&](study_ui::Rect bounds,study_image_quad::Tint color,float radius=4) {
        study_rounded::Draw draw;
        draw.image.rect={float(bounds.x),float(bounds.y),float(bounds.w),float(bounds.h)};
        draw.image.tint=color;draw.radius=radius;
        return images.draw(rounded,white,draw);
    };
    using namespace study_menu;
    const auto start=study_ui::button(start_bounds,pointer);
    const auto check=study_ui::checkbox(decorations_bounds,pointer,preferences.decorations());
    const auto selector=study_ui::selector(badge_bounds,pointer,preferences.badge(),Preferences::badge_count);
    if (!rectangle(start_bounds,control_color(start,focus==Focus::start)) ||
        !labels.centered(Label::start,text,46,82)) return false;
    if (!rectangle(decorations_bounds,control_color(check.interaction,focus==Focus::decorations)) ||
        !rectangle({114,88,16,16},{.75f,.80f,.9f,1},2) ||
        !rectangle({116,90,12,12},check.checked ? study_image_quad::Tint{.20f,.55f,.85f,1}
                                              : study_image_quad::Tint{.06f,.08f,.12f,1},1) ||
        !labels.centered(Label::decorations,text,207,102)) return false;
    const study_ui::Rect previous{badge_bounds.x,badge_bounds.y,badge_bounds.h,badge_bounds.h};
    const study_ui::Rect next{badge_bounds.x+badge_bounds.w-badge_bounds.h,badge_bounds.y,badge_bounds.h,badge_bounds.h};
    if (!rectangle(badge_bounds,focus==Focus::badge ? study_image_quad::Tint{.30f,.25f,.09f,1}
                                                  : study_image_quad::Tint{.10f,.14f,.21f,1}) ||
        !rectangle(previous,control_color(selector.previous,focus==Focus::badge,selector.previous_enabled)) ||
        !rectangle(next,control_color(selector.next,focus==Focus::badge,selector.next_enabled))) return false;
    // Hide the unavailable arrow as well as dimming its background, so the
    // endpoint is distinguishable without relying on color alone.
    if (selector.previous_enabled && !labels.centered(Label::previous,text,122,142)) return false;
    if (selector.next_enabled && !labels.centered(Label::next,text,278,142)) return false;
    const auto focused_bounds=focus==Focus::start ? start_bounds
        : focus==Focus::decorations ? decorations_bounds : badge_bounds;
    if (!rectangle({focused_bounds.x,focused_bounds.y+focused_bounds.h-2,focused_bounds.w,2},
                   {.95f,.80f,.25f,1},0)) return false;
    return labels.centered(preferences.badge()==0 ? Label::original : Label::alternate,text,200,142);
}
} // namespace study_menu_render
