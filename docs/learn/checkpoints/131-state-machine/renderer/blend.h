// renderer/blend.h
// Tiny CPU compositing helper for bounded LDR linear-channel RGBA samples.
//
// Conventions:
// - Inputs are finite and normalized to [0, 1]. Nothing is clamped or rounded
//   here; invalid samples are rejected by returning std::nullopt instead.
// - "Straight" (non-premultiplied) color stores RGB independent of alpha.
// - "Premultiplied" color stores RGB already scaled by alpha, so every RGB
//   channel must satisfy channel <= alpha.
// - Outputs are stored in premultiplied form, matching the source-over equation.
//   Recover straight RGB only when alpha > 0; at alpha == 0 its value is undefined.
//   The RGB <= alpha policy is for this nonnegative LDR model, not HDR colors.
// - This models bounded LDR linear-channel arithmetic, not sRGB transfer
//   conversion or hardware-specific rounding. No exceptions are thrown.

#ifndef STUDY_BLEND_H
#define STUDY_BLEND_H

#include <cmath>
#include <optional>

namespace study_blend {

struct Rgba {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 0.0;
};

namespace detail {

inline bool finite_unit(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

}  // namespace detail

// A straight sample is valid when every component is finite and within [0, 1].
inline bool valid_rgba(const Rgba& c) noexcept {
    return detail::finite_unit(c.r) && detail::finite_unit(c.g) &&
           detail::finite_unit(c.b) && detail::finite_unit(c.a);
}

// A premultiplied sample is valid when it is a valid straight sample and every
// RGB channel does not exceed alpha. Violations are rejected, never clamped.
inline bool valid_premultiplied(const Rgba& c) noexcept {
    return valid_rgba(c) && c.r <= c.a && c.g <= c.a && c.b <= c.a;
}

// Convert a straight sample to premultiplied form. Returns std::nullopt when
// the input is not a valid straight sample.
inline std::optional<Rgba> premultiply(const Rgba& straight) noexcept {
    if (!valid_rgba(straight)) {
        return std::nullopt;
    }
    Rgba out;
    out.r = straight.r * straight.a;
    out.g = straight.g * straight.a;
    out.b = straight.b * straight.a;
    out.a = straight.a;
    return out;
}

// Source-over blend with premultiplied source and destination.
// Result RGB = src.rgb + dst.rgb * (1 - src.a),
// result alpha = src.a + dst.a * (1 - src.a).
// Returns std::nullopt when either operand is not valid premultiplied RGBA.
inline std::optional<Rgba> over_premultiplied(const Rgba& src, const Rgba& dst) noexcept {
    if (!valid_premultiplied(src) || !valid_premultiplied(dst)) {
        return std::nullopt;
    }
    const double keep = 1.0 - src.a;
    Rgba out;
    out.r = src.r + dst.r * keep;
    out.g = src.g + dst.g * keep;
    out.b = src.b + dst.b * keep;
    out.a = src.a + dst.a * keep;
    return out;
}

// Source-over blend with a straight source and premultiplied destination.
// The source RGB is multiplied by its alpha exactly once, then the
// premultiplied entry point performs the blend. Returns std::nullopt when
// either operand is invalid; the destination must already be premultiplied.
inline std::optional<Rgba> over_straight(const Rgba& src, const Rgba& dst) noexcept {
    const std::optional<Rgba> premul_src = premultiply(src);
    if (!premul_src.has_value()) {
        return std::nullopt;
    }
    return over_premultiplied(*premul_src, dst);
}

}  // namespace study_blend

#endif  // STUDY_BLEND_H
