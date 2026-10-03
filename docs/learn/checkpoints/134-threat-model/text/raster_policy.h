#ifndef FONT_RASTER_POLICY_H
#define FONT_RASTER_POLICY_H

// Quantize a drawable/logical scale and bound the requested font metric height
// before conversion or key packing. Height denotes ascent-descent at the chosen
// scale, not em size or an individual glyph's bitmap row count.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace font_raster {

// Largest logical or device metric height accepted by this resource policy.
inline constexpr int kMaxHeight = 2048;

// Density-independent render scale ceiling; keeps scale_quantized and the
// resulting product comfortably inside finite double range.
inline constexpr double kMaxRenderScale = 2048.0;

// Number of quantization steps per render-scale unit (one eighth).
inline constexpr double kScaleStep = 8.0;

struct Plan {
    int logical_height;      // requested metric height in logical pixels
    int device_height;       // requested metric height for the rasterizer
    double scale_quantized;  // render_scale snapped to the nearest eighth
    std::uint64_t key;       // packed (scalar, logical, device) identity
};

inline std::optional<Plan> plan(std::uint32_t scalar,
                                int logical_height,
                                double render_scale) noexcept {
    // Reject malformed Unicode scalars, including UTF-16 surrogate code points,
    // before any arithmetic is attempted.
    if (scalar > 0x10FFFFu) {
        return std::nullopt;
    }
    if (scalar >= 0xD800u && scalar <= 0xDFFFu) {
        return std::nullopt;
    }

    // Logical height and render scale must both lie in the accepted domain.
    if (logical_height < 1 || logical_height > kMaxHeight) {
        return std::nullopt;
    }
    if (!std::isfinite(render_scale) || render_scale <= 0.0 ||
        render_scale > kMaxRenderScale) {
        return std::nullopt;
    }

    // Snap the raw scale to the nearest eighth, never below one. The rounded
    // device height need not be an integer multiple of the logical height.
    const double scale_quantized =
        std::max(1.0, std::round(render_scale * kScaleStep) / kScaleStep);

    // Both operands are bounded (logical <= 2048, scale <= 2048), so this
    // product is finite. The rounded device height is range-checked BEFORE the
    // cast to int; the packing below then does not rely on narrowing to prove
    // anything about validity.
    const double scaled =
        std::round(static_cast<double>(logical_height) * scale_quantized);
    if (scaled < 1.0 || scaled > static_cast<double>(kMaxHeight)) {
        return std::nullopt;
    }
    const int device_height = static_cast<int>(scaled);

    // Packing is exact and injective over the accepted domain: the scalar
    // occupies the high 32 bits, the logical height the next 16, and the device
    // height the low 16. Distinct raw scales that snap to the same integer
    // device height intentionally reuse the same key.
    const std::uint64_t key =
        (static_cast<std::uint64_t>(scalar) << 32) |
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(logical_height)) << 16) |
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(device_height));

    return Plan{logical_height, device_height, scale_quantized, key};
}

}  // namespace font_raster

#endif  // FONT_RASTER_POLICY_H
