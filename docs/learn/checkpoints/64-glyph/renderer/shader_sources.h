#pragma once
// Stable teaching sources: matching stage interfaces; linking does not perform a draw.
namespace study_shader {
inline constexpr char vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
out vec3 v_color;
void main() {
    v_color = vec3(0.2, 0.7, 0.9);
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)glsl";
inline constexpr char fragment[] = R"glsl(#version 330 core
in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = vec4(v_color, 1.0);
}
)glsl";
} // namespace study_shader
