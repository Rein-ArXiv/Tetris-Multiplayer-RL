#pragma once
#include "presentation/image_fit.h"
#include <cmath>
#include "renderer/image_store.h"
namespace study_art {
// A valid image too thin for the integer layout is deliberately omitted.
// Handles are borrowed; an invalid/stale handle or GPU draw failure is an error.
inline bool draw_contained(study_image::ImageStore& images,
                           study_image_quad::ImageQuad& quad,
                           study_image::Handle image,image_fit::Rect box,double opacity=1.0) {
    if(!std::isfinite(opacity)||opacity<0.0||opacity>1.0) return false;
    int width=0,height=0;
    if(!images.size(image,width,height)) return false;
    const auto fitted=image_fit::contain(box,width,height);
    if(!fitted) return true;
    study_image_quad::Draw request;
    request.rect={float(fitted->x),float(fitted->y),float(fitted->width),float(fitted->height)};
    request.tint.a=static_cast<float>(opacity);
    return images.draw(quad,image,request);
}
} // namespace study_art
