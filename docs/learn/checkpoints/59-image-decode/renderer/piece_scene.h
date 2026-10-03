#pragma once
#include "piece_geometry.h"
#include "triangle.h"
namespace study_piece_view {
inline constexpr char fragment[] = R"glsl(#version 330 core
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(0,0.75,1,1); }
)glsl";
// Borrow the board pass's viewport/scissor/opaque state. Never clear here.
// available matches the uploaded used prefix. Zero means no GL objects needed.
inline bool render(const study_gl::GlApi& gl,study_gl::GLuint program,
                   study_gl::GLuint vao,study_gl::GLsizei available) noexcept {
    if(available<0 || available>24 || available%6!=0)return false;
    if(available==0)return true;
    return study_gl::submit_triangles(gl,program,vao,available,0,available);
}
} // namespace study_piece_view
