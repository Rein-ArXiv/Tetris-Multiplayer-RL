#pragma once
#include "renderer/image_geometry.h"
#include <algorithm>
namespace study_rounded {
struct Draw {
    study_image_quad::Draw image;
    float radius=0; // Logical units, independent of UV and tint.
};
struct Vertex {
    float x,y,u,v,r,g,b,a;
    float local_x,local_y,half_width,half_height,radius;
};
static_assert(std::is_standard_layout_v<Vertex> && sizeof(Vertex)==52);
static_assert(offsetof(Vertex,u)==8 && offsetof(Vertex,r)==16);
static_assert(offsetof(Vertex,local_x)==32 && offsetof(Vertex,half_width)==40 && offsetof(Vertex,radius)==48);
// Limit UI extent so length(q) in float GLSL cannot overflow. This is an API
// policy, not a physical-pixel limit. The position/rotation contract is unchanged.
inline std::optional<std::array<Vertex,6>> make_vertices(const Draw& draw) noexcept {
    const auto& rect=draw.image.rect;
    if(!std::isfinite(draw.radius) || draw.radius<0 || rect.width>1e6f || rect.height>1e6f ||
       draw.radius>(std::min)(rect.width,rect.height)*.5f) return std::nullopt;
    const auto base=study_image_quad::make_vertices(draw.image);
    if(!base)return std::nullopt;
    const float hw=rect.width*.5f,hh=rect.height*.5f;
    const float sx[6]={-1,-1,1,-1,1,1};
    const float sy[6]={-1,1,1,-1,1,-1};
    std::array<Vertex,6> result{};
    for(std::size_t i=0;i<result.size();++i){
        const auto& v=(*base)[i];
        result[i]={v.x,v.y,v.u,v.v,v.r,v.g,v.b,v.a,sx[i]*hw,sy[i]*hh,hw,hh,draw.radius};
    }
    return result;
}
} // namespace study_rounded
