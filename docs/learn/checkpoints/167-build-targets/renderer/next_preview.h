#pragma once
#include "simulation/next_queue.h"
#include "board_geometry.h"
#include "triangle.h"
namespace study_next_view {
struct Mesh {
    std::array<study_mesh::Vertex2,study_next::Queue::capacity*24> vertices{};
    std::size_t count=0;
};
// Three independent spawn-shape thumbnails; no board origin or collision query.
inline std::optional<Mesh> make_mesh(const study_next::Queue& queue) noexcept {
    Mesh mesh;
    for(std::size_t slot=0;slot<queue.size();++slot){
        const auto kind=queue.peek(slot);
        const auto* definition=kind?study_catalog::find(*kind):nullptr;
        if(!definition)return std::nullopt;
        for(auto cell:definition->cells){
            const double x=244.0+cell.column*6.0;
            const double y=40.0+slot*60.0+cell.row*6.0;
            for(auto vertex:study_board::quad_vertices({x,y,x+5.0,y+5.0}))
                mesh.vertices[mesh.count++]=vertex;
        }
    }
    return mesh;
}
// Borrow the board pass's state; the same cyan program can draw these shapes.
inline bool render(const study_gl::GlApi& gl,study_gl::GLuint program,
                   study_gl::GLuint vao,study_gl::GLsizei count) noexcept {
    if(count<0||count>72||count%24!=0)return false;
    return count==0 || study_gl::submit_triangles(gl,program,vao,count,0,count);
}
} // namespace study_next_view
