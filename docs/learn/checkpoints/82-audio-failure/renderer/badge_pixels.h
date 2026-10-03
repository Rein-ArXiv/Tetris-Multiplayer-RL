#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "rgba_pixels.h"

namespace study_texture {

inline constexpr int badge_width = 8;
inline constexpr int badge_height = 8;

namespace detail {

// Builds a simple 8x8 player face icon as tight RGBA8 bytes.
// Rows run top to bottom and pixels run left to right.
// Every pixel is fully opaque (alpha 255). Row 0 keeps its two leftmost pixels
// red so tests can detect row/column orientation.
constexpr std::array<uint8_t, 256> make_badge_pixels() noexcept {
    constexpr uint8_t background_r = 20;
    constexpr uint8_t background_g = 32;
    constexpr uint8_t background_b = 56;

    constexpr uint8_t face_r = 58;
    constexpr uint8_t face_g = 190;
    constexpr uint8_t face_b = 220;

    constexpr uint8_t eye_r = 245;
    constexpr uint8_t eye_g = 245;
    constexpr uint8_t eye_b = 245;

    constexpr uint8_t mark_r = 230;
    constexpr uint8_t mark_g = 60;
    constexpr uint8_t mark_b = 70;

    std::array<uint8_t, 256> pixels{};

    for (int y = 0; y < badge_height; ++y) {
        for (int x = 0; x < badge_width; ++x) {
            // Direction marker: only the two leftmost pixels of the top row.
            const bool is_direction_mark = (y == 0) && (x < 2);

            // Two 1x2 eyes inside the face.
            const bool is_eye = ((x == 2) || (x == 5)) && ((y == 2) || (y == 3));

            // Rounded 6x6 face with the four corners removed.
            const bool is_face = (x >= 1) && (x <= 6) && (y >= 1) && (y <= 6) &&
                                 !(((x == 1) || (x == 6)) && ((y == 1) || (y == 6)));

            uint8_t r = background_r;
            uint8_t g = background_g;
            uint8_t b = background_b;

            if (is_face) {
                r = face_r;
                g = face_g;
                b = face_b;
            }
            if (is_eye) {
                r = eye_r;
                g = eye_g;
                b = eye_b;
            }
            if (is_direction_mark) {
                r = mark_r;
                g = mark_g;
                b = mark_b;
            }

            const std::size_t offset =
                static_cast<std::size_t>((y * badge_width + x) * 4);
            pixels[offset + 0] = r;
            pixels[offset + 1] = g;
            pixels[offset + 2] = b;
            pixels[offset + 3] = 255;
        }
    }

    return pixels;
}

} // namespace detail

// Single source of truth for the icon; generated rather than written out.
inline constexpr std::array<uint8_t, 256> badge_pixels = detail::make_badge_pixels();

} // namespace study_texture
