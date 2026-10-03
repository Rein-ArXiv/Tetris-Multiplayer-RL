#pragma once
#include <cstdint>
#include <limits>
#include <optional>

namespace image_fit {
struct Rect { int x=0, y=0, width=0, height=0; };
static_assert(std::numeric_limits<int>::digits<=31,"image layout requires at most 32-bit int");
// Validate before derived int coordinates reach the drawing API.
inline bool valid(Rect box) noexcept {
    return box.width>0 && box.height>0 &&
        std::int64_t(box.x)+box.width<=(std::numeric_limits<int>::max)() &&
        std::int64_t(box.y)+box.height<=(std::numeric_limits<int>::max)();
}
// Contain the whole source, centered. Floor the fitted side. If the thin side
// would collapse below one logical unit, omit the image instead of stretching it.
inline std::optional<Rect> contain(Rect box,int source_width,int source_height) noexcept {
    if(!valid(box)||source_width<=0||source_height<=0) return std::nullopt;
    int width,height;
    if(std::int64_t(box.width)*source_height<=std::int64_t(box.height)*source_width) {
        width=box.width;
        height=int(std::int64_t(box.width)*source_height/source_width);
    } else {
        height=box.height;
        width=int(std::int64_t(box.height)*source_width/source_height);
    }
    if(width<=0||height<=0) return std::nullopt;
    return Rect{box.x+(box.width-width)/2,box.y+(box.height-height)/2,width,height};
}
} // namespace image_fit
