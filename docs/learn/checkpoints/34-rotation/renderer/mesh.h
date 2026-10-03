#pragma once
// Lesson 13: CPU vertex-data checkpoint.
// CPU positions only. No GL headers, no GL calls, no NDC/screen-output claims.

#include <array>
#include <climits>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace study_mesh {

struct Vertex2 {
    float x;
    float y;
};

using Triangle = std::array<Vertex2, 3>;

constexpr Triangle make_triangle() {
    return Triangle{{
        Vertex2{-0.5f, -0.5f},
        Vertex2{ 0.5f, -0.5f},
        Vertex2{ 0.0f,  0.5f},
    }};
}

constexpr std::size_t byte_count(const Triangle& vertices) {
    return vertices.size() * sizeof(Vertex2);
}

} // namespace study_mesh

// --- Future GL_FLOAT target contract ---------------------------------
// Verified assumptions we will rely on once a shader and vertex attribute
// layout exist. Nothing here uploads data or touches a GPU.
static_assert(CHAR_BIT == 8, "assume 8-bit bytes");
static_assert(sizeof(float) == 4, "assume 32-bit IEEE-754 float");
static_assert(std::numeric_limits<float>::is_iec559, "assume IEC 559 float");

static_assert(std::is_standard_layout<study_mesh::Vertex2>::value,
              "Vertex2 must be standard-layout");
static_assert(std::is_trivially_copyable<study_mesh::Vertex2>::value,
              "Vertex2 must be trivially copyable");

static_assert(offsetof(study_mesh::Vertex2, x) == 0,
              "x at offset 0");
static_assert(offsetof(study_mesh::Vertex2, y) == sizeof(float),
              "y directly after x");
static_assert(sizeof(study_mesh::Vertex2) == 2 * sizeof(float),
              "no padding in Vertex2");
