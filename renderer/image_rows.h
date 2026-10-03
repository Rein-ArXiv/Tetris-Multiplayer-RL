#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace image_detail {

// Bytes required for a width x height RGBA8 buffer, or nullopt when the
// dimensions are not positive or the byte count is not representable.
inline std::optional<std::size_t> rgba_storage_bytes(int width, int height) noexcept {
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    const std::size_t max_size = (std::numeric_limits<std::size_t>::max)();

    // Divide before multiplying so neither product can wrap.
    if (w > max_size / h) {
        return std::nullopt;
    }
    const std::size_t pixels = w * h;
    if (pixels > max_size / 4u) {
        return std::nullopt;
    }
    const std::size_t bytes = pixels * 4u;

    // The output length must also fit in ptrdiff_t for pointer arithmetic.
    const std::size_t max_ptrdiff =
        static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
    if (bytes > max_ptrdiff) {
        return std::nullopt;
    }
    return bytes;
}

// Copies a BGRA image into a tightly packed RGBA buffer. scan0 is the first
// pixel of the first logical row and the next logical row begins at
// scan0 + stride; stride may be negative. Row padding is ignored and alpha
// is preserved. out_bytes must equal rgba_storage_bytes(width, height).
// Returns false, leaving out untouched, when any input or arithmetic check
// fails. Non-overlapping, accessible row memory is the caller contract.
inline bool copy_bgra_rows(const std::uint8_t* scan0, std::ptrdiff_t stride,
                           int width, int height, std::uint8_t* out,
                           std::size_t out_bytes) noexcept {
    if (scan0 == nullptr || out == nullptr) {
        return false;
    }

    const std::optional<std::size_t> storage = rgba_storage_bytes(width, height);
    if (!storage || out_bytes != *storage) {
        return false;
    }

    // Negating the most negative stride would overflow.
    if (stride == (std::numeric_limits<std::ptrdiff_t>::min)()) {
        return false;
    }
    const std::ptrdiff_t abs_stride = stride < 0 ? -stride : stride;

    // Bytes per row; this already fits because width * height * 4 does.
    const std::ptrdiff_t row_bytes = static_cast<std::ptrdiff_t>(width) * 4;
    if (abs_stride < row_bytes) {
        return false;
    }

    const std::ptrdiff_t max_ptrdiff = (std::numeric_limits<std::ptrdiff_t>::max)();
    const std::ptrdiff_t last_row = static_cast<std::ptrdiff_t>(height) - 1;

    // (height - 1) * abs_stride must not overflow, and the final row must
    // still fit along with its width.
    if (last_row > max_ptrdiff / abs_stride) {
        return false;
    }
    const std::ptrdiff_t span = last_row * abs_stride;
    if (span > max_ptrdiff - row_bytes) {
        return false;
    }

    const std::size_t row_bytes_sz = static_cast<std::size_t>(row_bytes);
    for (int y = 0; y < height; ++y) {
        const std::ptrdiff_t src_offset = static_cast<std::ptrdiff_t>(y) * stride;
        const std::uint8_t* src = scan0 + src_offset;
        std::uint8_t* dst = out + static_cast<std::size_t>(y) * row_bytes_sz;
        for (std::size_t x = 0; x < row_bytes_sz; x += 4) {
            dst[x + 0] = src[x + 2];  // R
            dst[x + 1] = src[x + 1];  // G from G
            dst[x + 2] = src[x + 0];  // B
            dst[x + 3] = src[x + 3];  // A preserved
        }
    }
    return true;
}

}  // namespace image_detail
