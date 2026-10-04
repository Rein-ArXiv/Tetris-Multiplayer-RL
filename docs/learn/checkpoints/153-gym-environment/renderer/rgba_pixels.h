#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace study_texture {

using std::uint8_t;

// Tight, non-owning RGBA8 view. Rows run top to bottom and pixels run left to
// right, with R, G, B and A each taking exactly one byte.
struct RgbaView {
    const std::uint8_t* data;
    std::size_t bytes;
    int width;
    int height;
};

// Returns width * height * 4 for positive dimensions, or std::nullopt when a
// dimension is not positive or the byte count would overflow std::size_t.
// Overflow is rejected up front with division before any multiplication.
constexpr std::optional<std::size_t> rgba_byte_count(int width, int height) noexcept {
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }

    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    constexpr std::size_t kMaxSize = static_cast<std::size_t>(-1);

    if (w > kMaxSize / h) {
        return std::nullopt;
    }

    const std::size_t pixel_count = w * h;
    if (pixel_count > kMaxSize / 4) {
        return std::nullopt;
    }

    return pixel_count * 4;
}

// A view is valid when the pointer is non-null, the dimensions are positive and
// the declared byte count matches width * height * 4 exactly. Padding bytes and
// extra trailing bytes are rejected, so only tight RGBA8 buffers pass.
// Whether data actually addresses `bytes` readable bytes is a caller contract;
// this function does not check it and cannot guarantee it.
constexpr bool valid(RgbaView v) noexcept {
    if (v.data == nullptr) {
        return false;
    }

    const std::optional<std::size_t> expected = rgba_byte_count(v.width, v.height);
    if (!expected.has_value()) {
        return false;
    }

    return v.bytes == expected.value();
}

} // namespace study_texture
