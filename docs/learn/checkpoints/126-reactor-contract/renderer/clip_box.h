#ifndef STUDY_FLUSH_RENDERER_CLIP_BOX_H
#define STUDY_FLUSH_RENDERER_CLIP_BOX_H

#include "renderer/letterbox.h"

#include <cstdint>
#include <limits>
#include <optional>

namespace study_flush {

// Integer pixel clip rectangle.
struct Clip {
    int x, y, width, height;
};

inline bool operator==(Clip a, Clip b) noexcept {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

inline bool operator!=(Clip a, Clip b) noexcept { return !(a == b); }

// True when every field is non-negative and x+width / y+height cannot overflow.
inline bool valid(Clip c) noexcept {
    if (c.x < 0 || c.y < 0 || c.width < 0 || c.height < 0) return false;
    if (c.x > std::numeric_limits<int>::max() - c.width) return false;
    if (c.y > std::numeric_limits<int>::max() - c.height) return false;
    return true;
}

// Both mapping helpers require an unchanged Layout returned by make_layout.
// Viewport mapped into GL bottom-left coordinates.
inline Clip full_clip(const study_letterbox::Layout& l) noexcept {
    return Clip{l.viewport.x, study_letterbox::gl_y(l), l.viewport.width, l.viewport.height};
}

// Project a logical pixel rect onto the viewport, rounding outward conservatively.
inline std::optional<Clip> project_clip(const study_letterbox::Layout& l,
                                        study_letterbox::Rect r) noexcept {
    if (r.x < 0 || r.y < 0 || r.width <= 0 || r.height <= 0) return std::nullopt;
    if (r.x > l.logical.width || r.y > l.logical.height) return std::nullopt;
    if (r.width > l.logical.width - r.x || r.height > l.logical.height - r.y)
        return std::nullopt;

    const std::int64_t lw = l.logical.width;
    const std::int64_t lh = l.logical.height;

    // Inputs are non-negative, so truncation equals floor.
    const std::int64_t left =
        static_cast<std::int64_t>(r.x) * l.viewport.width / lw;
    const std::int64_t top =
        static_cast<std::int64_t>(r.y) * l.viewport.height / lh;
    const std::int64_t right =
        ((static_cast<std::int64_t>(r.x) + r.width) * l.viewport.width + lw - 1) / lw;
    const std::int64_t bottom =
        ((static_cast<std::int64_t>(r.y) + r.height) * l.viewport.height + lh - 1) / lh;

    const std::int64_t x0 = static_cast<std::int64_t>(l.viewport.x) + left;
    const std::int64_t y = static_cast<std::int64_t>(l.drawable.height) -
                           (static_cast<std::int64_t>(l.viewport.y) + bottom);

    return Clip{static_cast<int>(x0), static_cast<int>(y),
                static_cast<int>(right - left), static_cast<int>(bottom - top)};
}

}  // namespace study_flush

#endif  // STUDY_FLUSH_RENDERER_CLIP_BOX_H

