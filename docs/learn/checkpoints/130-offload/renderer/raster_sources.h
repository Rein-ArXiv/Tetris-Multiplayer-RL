#pragma once
#include <string_view>
namespace study_raster_shader {
// These experiments require DrawArrays(Triangles, 0, 3), equal clip w=1,
// a new context's default LAST_VERTEX_CONVENTION, and no MSAA.
inline constexpr char smooth_vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
smooth out vec3 v_color;
void main() {
    const vec3 colors[3] = vec3[3](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
    v_color = colors[gl_VertexID];
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)glsl";
inline constexpr char smooth_fragment[] = R"glsl(#version 330 core
smooth in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(v_color, 1.0); }
)glsl";
inline constexpr char flat_vertex[] = R"glsl(#version 330 core
layout(location = 0) in vec2 a_position;
flat out vec3 v_color;
void main() {
    const vec3 colors[3] = vec3[3](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
    v_color = colors[gl_VertexID];
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)glsl";
inline constexpr char flat_fragment[] = R"glsl(#version 330 core
flat in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(v_color, 1.0); }
)glsl";
inline constexpr char cutout_fragment[] = R"glsl(#version 330 core
smooth in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() {
    ivec2 cell = ivec2(floor(gl_FragCoord.xy / 8.0));
    if ((cell.x + cell.y) % 2 == 0) discard;
    out_color = vec4(v_color, 1.0);
}
)glsl";
struct Case { const char* name; const char* vertex_source; const char* fragment_source; };
inline constexpr Case cases[] = {
    {"smooth",smooth_vertex,smooth_fragment},
    {"flat",flat_vertex,flat_fragment},
    {"cutout",smooth_vertex,cutout_fragment}
};
inline const Case* find_case(std::string_view name) noexcept {
    for (const auto& c : cases) if (name == c.name) return &c;
    return nullptr;
}
} // namespace study_raster_shader
