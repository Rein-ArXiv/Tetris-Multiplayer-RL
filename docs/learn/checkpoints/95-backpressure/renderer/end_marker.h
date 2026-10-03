#pragma once
#include "board_geometry.h"
#include "triangle.h"
namespace study_end_view {
// Opaque X to the left of the board: shape conveys ended state as well as color.
inline std::array<study_mesh::Vertex2,12> make_mesh() noexcept {
    constexpr double points[12][2]={
        {30,86},{36,80},{70,114},{30,86},{70,114},{64,120},
        {64,80},{70,86},{36,120},{64,80},{36,120},{30,114}
    };
    std::array<study_mesh::Vertex2,12> vertices{};
    for(std::size_t i=0;i<vertices.size();++i){
        const auto clip=study_coordinates::ui_to_clip(points[i][0],points[i][1],320,240);
        vertices[i]={static_cast<float>(clip->x),static_cast<float>(clip->y)};
    }
    return vertices;
}
inline constexpr char fragment[]=R"glsl(#version 330 core
layout(location=0) out vec4 out_color;
void main(){out_color=vec4(1,0.125,0.125,1);}
)glsl";
// Borrow the board pass state. Do not clear or obscure the final board.
inline bool render(const study_gl::GlApi& gl,study_gl::GLuint program,
                   study_gl::GLuint vao,study_gl::GLsizei count) noexcept {
    return count==12 && study_gl::submit_triangles(gl,program,vao,count,0,count);
}
} // namespace study_end_view
