#pragma once
// Stable teaching sources: compile each stage now, link/use in the next step.
namespace study_shader {
inline constexpr char vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)glsl";
inline constexpr char fragment[] = R"glsl(#version 330 core
out vec4 out_color;
void main() {
    out_color = vec4(0.2, 0.7, 0.9, 1.0);
}
)glsl";
} // namespace study_shader
