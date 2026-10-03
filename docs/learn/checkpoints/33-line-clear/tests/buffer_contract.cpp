#include "renderer/vertex_buffer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>
#include <limits>
#include "renderer/quad.h"
using namespace study_gl;
static_assert(!std::is_copy_constructible<VertexBuffer>::value);
static_assert(!std::is_move_constructible<VertexBuffer>::value);
namespace {
int mode, generated, deleted, uploaded;
GLsizeiptr expected_bytes=24;
GLenum pending;
GLuint binding;
bool context_alive;
std::vector<unsigned char> storage;
void require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr,"buffer contract: %s\n",why); std::exit(1); }
}
GLenum STUDY_GL_CALL error() { const auto value=pending; pending=0; return value; }
void STUDY_GL_CALL gen(GLsizei n, GLuint* value) {
    require(context_alive && n==1,"generation boundary"); ++generated;
    if (mode==1) { pending=0x0502; return; }
    if (mode==2) return; // Defensive zero-name path, not an API success guarantee.
    *value=17;
}
void STUDY_GL_CALL bind(GLenum target, GLuint name) {
    require(context_alive && target==ArrayBuffer && name==17,"binding");
    if (mode==3) { pending=0x0502; return; }
    binding=name;
}
void STUDY_GL_CALL data(GLenum target, GLsizeiptr bytes, const void* source, GLenum usage) {
    require(context_alive && binding==17 && target==ArrayBuffer,"upload boundary");
    require(bytes==expected_bytes && source && usage==StaticDraw,"byte count/source/hint"); ++uploaded;
    if (mode==4) { pending=0x0502; return; }
    storage.resize(static_cast<std::size_t>(bytes));
    std::memcpy(storage.data(),source,storage.size());
}
void STUDY_GL_CALL remove(GLsizei n,const GLuint* name) {
    require(context_alive && n==1 && *name==17,"delete before context death");
    ++deleted; binding=0; storage.clear();
}
}
int main() {
    GlApi gl; gl.GetError=error; gl.GenBuffers=gen; gl.BindBuffer=bind;
    gl.BufferData=data; gl.DeleteBuffers=remove;
    for (mode=0; mode<=5; ++mode) {
        context_alive=true; generated=deleted=uploaded=0; binding=0; storage.clear();
        pending=(mode==5 ? 0x0500 : 0);
        {
            VertexBuffer buffer(gl);
            auto vertices=study_mesh::make_triangle();
            const bool ok=buffer.upload(vertices);
            require(ok==(mode==0),"failure result");
            if (ok) {
                require(buffer.name()==17 && buffer.vertex_count()==3 && storage.size()==24,"published name and storage");
                vertices[0].x=9;
                study_mesh::Triangle readback{};
                std::memcpy(readback.data(),storage.data(),storage.size());
                require(readback[0].x==-0.5f,"CPU source is copied");
                require(!buffer.upload(vertices) && generated==1 && deleted==0,"repeat upload preserves owner");
                buffer.reset(); buffer.reset();
                require(deleted==1 && !buffer.name() && buffer.vertex_count()==0,"idempotent reset");
                require(buffer.upload(vertices),"upload after reset");
            } else {
                require(!buffer.name() && buffer.vertex_count()==0,"failure leaves empty owner");
                require(uploaded==(mode==4 ? 1 : 0),"no work after failed stage");
            }
        }
        require(deleted==(mode==0 ? 2 : (mode==3 || mode==4 ? 1 : 0)),"exact cleanup count");
        require(generated==(mode==5 ? 0 : (mode==0 ? 2 : 1)),"generation count");
        context_alive=false;
    }
    context_alive=true;mode=0;pending=0;generated=deleted=uploaded=0;
    {
        VertexBuffer buffer(gl);
        study_mesh::Vertex2 vertex{};
        require(!buffer.upload(nullptr,6),"null input");
        require(!buffer.upload(&vertex,0),"empty input");
        require(!buffer.upload(&vertex,std::numeric_limits<std::size_t>::max()),"overflow input");
        require(generated==0 && buffer.vertex_count()==0,"reject before allocation");
        expected_bytes=48;
        require(buffer.upload(study_quad::make_quad()),"quad upload");
        require(buffer.vertex_count()==6 && storage.size()==48,"quad count and bytes");
        require(!buffer.upload(&vertex,1) && buffer.vertex_count()==6,"rejected repeat preserves count");
    }
    require(deleted==1,"quad released once");context_alive=false;
    std::puts("Buffer owner: success, pending error, generation/error/zero, bind/upload failure, retry and cleanup passed");
}
