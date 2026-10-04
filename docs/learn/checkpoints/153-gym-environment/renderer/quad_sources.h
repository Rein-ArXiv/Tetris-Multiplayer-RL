#pragma once
#include <string_view>
namespace study_quad_shader {
inline constexpr char vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
flat out vec3 v_color;
void main() {
    // No array lookup: the six-vertex experiment uses one color per triangle.
    v_color = gl_VertexID < 3 ? vec3(0.2,0.7,0.9) : vec3(0.95,0.65,0.2);
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)glsl";
inline constexpr char fragment[] = R"glsl(#version 330 core
flat in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(v_color, 1.0); }
)glsl";
struct Case { const char* name; bool reverse_first; bool cull; };
inline constexpr Case cases[] = {
    {"quad",false,false}, {"reverse",true,false},
    {"cull",false,true}, {"reverse-cull",true,true}
};
inline const Case* find_case(std::string_view name) noexcept {
    for(const auto& c:cases) if(name==c.name)return &c;
    return nullptr;
}
} // namespace study_quad_shader
