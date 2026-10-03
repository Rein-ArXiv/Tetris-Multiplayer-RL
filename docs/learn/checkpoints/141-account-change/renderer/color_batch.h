#ifndef RENDERER_COLOR_BATCH_H
#define RENDERER_COLOR_BATCH_H

// Pure-CPU triangle batch for study_batch: no GL, allocation, I/O or RNG.
// Append-only prefix; a failed call never mutates existing state.

#include "renderer/mesh.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace study_batch {

struct Color {
    float r, g, b, a;
};

struct Vertex {
    float x, y, r, g, b, a;
};

static_assert(std::is_standard_layout_v<Color>);
static_assert(std::is_trivially_copyable_v<Color>);
static_assert(std::is_standard_layout_v<Vertex>);
static_assert(std::is_trivially_copyable_v<Vertex>);
static_assert(sizeof(Vertex) == 24, "Vertex must be 24 bytes");
static_assert(offsetof(Vertex, x) == 0, "x offset");
static_assert(offsetof(Vertex, y) == 4, "y offset");
static_assert(offsetof(Vertex, r) == 8, "r offset");
static_assert(offsetof(Vertex, a) == 20, "a offset");

class Batch final {
public:
    static constexpr std::size_t capacity = 2304;

    std::size_t size() const noexcept { return used_; }

    const Vertex* data() const noexcept { return vertices_.data(); }

    void clear() noexcept { used_ = 0; }

    // Both prefixes already satisfy the Vertex contract. Self-append is valid:
    // the destination starts after the old prefix, and used_ changes last.
    bool append_batch(const Batch& other) noexcept {
        const auto count = other.used_;
        if (count > capacity - used_) return false;
        for (std::size_t i = 0; i < count; ++i)
            vertices_[used_ + i] = other.vertices_[i];
        used_ += count;
        return true;
    }

    // Append `count` positions from `points`, all sharing `color`.
    // Returns false and leaves the prefix and used_ untouched on violation.
    // count == 0 succeeds without reading points or color.
    // Else requires: points != nullptr, count % 3 == 0,
    // count <= capacity - used_, finite xy for all points, and an opaque
    // color (finite rgb in [0,1], alpha == 1). points is borrowed only for
    // this call and must not overlap this batch. No auto-flush on overflow.
    bool append(const study_mesh::Vertex2* points, std::size_t count, Color color) noexcept
    {
        if (count == 0) {
            return true;
        }
        if (points == nullptr || count % 3 != 0) {
            return false;
        }
        if (!std::isfinite(color.r) || color.r < 0.0f || color.r > 1.0f ||
            !std::isfinite(color.g) || color.g < 0.0f || color.g > 1.0f ||
            !std::isfinite(color.b) || color.b < 0.0f || color.b > 1.0f ||
            color.a != 1.0f) {
            return false;
        }
        // used_ <= capacity, so the subtraction cannot underflow.
        if (count > capacity - used_) {
            return false;
        }
        // Validate every xy before mutating the existing prefix.
        for (std::size_t i = 0; i < count; ++i) {
            if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) {
                return false;
            }
        }
        for (std::size_t i = 0; i < count; ++i) {
            vertices_[used_ + i] = Vertex{points[i].x, points[i].y,
                                          color.r, color.g, color.b, color.a};
        }
        used_ += count;
        return true;
    }

private:
    std::array<Vertex, capacity> vertices_{};
    std::size_t used_ = 0;
};

} // namespace study_batch

#endif // RENDERER_COLOR_BATCH_H

