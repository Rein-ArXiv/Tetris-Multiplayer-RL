#pragma once
#include "coordinates.h"
#include "shader_sources.h"
#include <string_view>

namespace study_projection {
// CPU coefficients predict positions; vertex_source is the program actually run.
// Both descriptions are kept explicit so a pixel experiment can compare them.
struct Case { const char* name; const char* vertex_source; double xy_scale, w; };
inline constexpr char w2_vertex[] = R"GLSL(#version 330 core
layout(location = 0) in vec2 a_position;
out vec3 v_color;
void main() {
    gl_Position = vec4(a_position, 0.0, 2.0);
    v_color = vec3(0.2, 0.7, 0.9);
}
)GLSL";
inline constexpr char scaled_vertex[] = R"GLSL(#version 330 core
layout(location = 0) in vec2 a_position;
out vec3 v_color;
void main() {
    gl_Position = 2.0 * vec4(a_position, 0.0, 1.0);
    v_color = vec3(0.2, 0.7, 0.9);
}
)GLSL";
inline constexpr char oversized_vertex[] = R"GLSL(#version 330 core
layout(location = 0) in vec2 a_position;
out vec3 v_color;
void main() {
    gl_Position = vec4(3.0 * a_position, 0.0, 1.0);
    v_color = vec3(0.2, 0.7, 0.9);
}
)GLSL";
inline constexpr Case cases[] = {
    {"base", study_shader::vertex, 1.0, 1.0},
    {"w2", w2_vertex, 1.0, 2.0},
    {"scaled", scaled_vertex, 2.0, 2.0},
    {"oversized", oversized_vertex, 3.0, 1.0}
};
inline const Case* find_case(std::string_view name) noexcept {
    for (const auto& c : cases) if (name == c.name) return &c;
    return nullptr;
}
inline study_coordinates::ClipPosition clip_for(const Case& c, double x, double y) noexcept {
    return {c.xy_scale*x, c.xy_scale*y, 0.0, c.w};
}
} // namespace study_projection
