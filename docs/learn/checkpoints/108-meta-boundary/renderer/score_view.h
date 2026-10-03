#pragma once
// Fixed seven-segment digit geometry for a uint64 score (no font rendering).

#include <array>
#include <cstddef>
#include <cstdint>

#include "board_geometry.h"
#include "triangle.h"

namespace study_score_view {

using study_mesh::Vertex2;

struct Mesh {
    static constexpr std::size_t capacity = 20 * 7 * 6;
    std::array<Vertex2, capacity> vertices{};
    std::size_t count = 0;
};

// Segment rectangles in digit-local space {left, top, right, bottom}.
// Indices: a top, b upper-right, c lower-right, d bottom,
//          e lower-left, f upper-left, g middle.
inline constexpr int kSegments[7][4] = {
    {1, 0, 5, 1},    // a
    {5, 1, 6, 5},    // b
    {5, 6, 6, 10},   // c
    {1, 10, 5, 11},  // d
    {0, 6, 1, 10},   // e
    {0, 1, 1, 5},    // f
    {1, 5, 5, 6},    // g
};

// Active segments per digit, bit i selects kSegments[i].
inline constexpr unsigned kDigitMask[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

// Emit the geometry of a value; 0 still renders as a single digit.
inline Mesh make_mesh(std::uint64_t value) noexcept {
    std::array<unsigned, 20> digits{};
    std::size_t used = 0;
    do {
        digits[used++] = static_cast<unsigned>(value % 10u);
        value /= 10u;
    } while (value != 0);

    Mesh mesh;
    for (std::size_t i = 0; i < used; ++i) {
        const unsigned digit = digits[used - 1 - i];
        const unsigned mask = kDigitMask[digit];
        const float x = 8.0f + 8.0f * static_cast<float>(i);
        const float y = 4.0f;
        for (unsigned seg = 0; seg < 7; ++seg) {
            if ((mask & (1u << seg)) == 0) {
                continue;
            }
            const int* r = kSegments[seg];
            const auto quad = study_board::quad_vertices({
                x + static_cast<float>(r[0]), y + static_cast<float>(r[1]),
                x + static_cast<float>(r[2]), y + static_cast<float>(r[3]),
            });
            for (const Vertex2& v : quad) {
                mesh.vertices[mesh.count++] = v;
            }
        }
    }
    return mesh;
}

// Solid colour fragment shader; alpha stays 1 for the opaque board pass.
inline constexpr char fragment[] = R"(#version 330 core
layout(location = 0) out vec4 out_color;
void main() {
    out_color = vec4(0.5, 1.0, 0.25, 1.0);
}
)";

// Submit the used prefix of a capacity-sized mesh; board pass owns state.
inline bool render(const study_gl::GlApi& gl, study_gl::GLuint program,
                   study_gl::GLuint vao, study_gl::GLsizei available,
                   study_gl::GLsizei used) noexcept {
    if (available != static_cast<study_gl::GLsizei>(Mesh::capacity) || used <= 0 ||
        used > available || used % 6 != 0) return false;
    return study_gl::submit_triangles(gl, program, vao, available, 0, used);
}

}  // namespace study_score_view
