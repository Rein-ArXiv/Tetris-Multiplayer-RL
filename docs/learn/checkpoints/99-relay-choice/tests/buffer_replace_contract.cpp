#include "renderer/vertex_buffer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
using namespace study_gl;
static int binds, writes, generated, deleted, fail;
static GLenum pending;
static std::vector<unsigned char> storage;
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "replace: %s\n", message); std::exit(1); }
}
static GLenum STUDY_GL_CALL error() { auto e = pending; pending = 0; return e; }
static void STUDY_GL_CALL gen(GLsizei n, GLuint* p) { require(n==1,"one name"); ++generated; *p=42; }
static void STUDY_GL_CALL bind(GLenum t, GLuint n) {
    require(t==ArrayBuffer && n==42,"same name"); ++binds;
    if(fail==1) pending=0x0502;
}
static void STUDY_GL_CALL data(GLenum t, GLsizeiptr bytes, const void* p, GLenum hint) {
    require(t==ArrayBuffer && bytes==24 && p,"extent");
    require(hint==(writes==0?StaticDraw:DynamicDraw),"usage is explicit");
    ++writes;
    if(fail==2) { pending=0x0505; return; }
    storage.resize(24); std::memcpy(storage.data(),p,24);
}
static void STUDY_GL_CALL remove(GLsizei n,const GLuint* p) { require(n==1 && *p==42,"owned deletion"); ++deleted; }
int main() {
    GlApi gl; gl.GetError=error; gl.GenBuffers=gen; gl.BindBuffer=bind;
    gl.BufferData=data; gl.DeleteBuffers=remove;
    for(int scenario=0;scenario<4;++scenario) {
        binds=writes=generated=deleted=fail=0;pending=0;storage.clear();
        {
            VertexBuffer buffer(gl);
            auto vertices=study_mesh::make_triangle();
            require(!buffer.replace_same_size(vertices.data(),3) && binds==0,"empty owner rejected");
            require(buffer.upload(vertices),"initial upload");
            auto old=storage;
            require(!buffer.replace_same_size(nullptr,3),"null rejected");
            require(!buffer.replace_same_size(vertices.data(),0),"zero rejected");
            require(!buffer.replace_same_size(vertices.data(),2),"different size rejected");
            require(!buffer.replace_same_size(vertices.data(),std::numeric_limits<std::size_t>::max()),"overflow rejected");
            require(binds==1 && writes==1 && storage==old,"invalid input does no GL work");
            vertices[0].x=0.25f;
            if(scenario==1)pending=0x0500;
            if(scenario==2)fail=1;
            if(scenario==3)fail=2;
            const bool ok=buffer.replace_same_size(vertices.data(),3);
            require(ok==(scenario==0),"stage result");
            require(generated==1 && deleted==0 && buffer.name()==42 && buffer.vertex_count()==3,"ownership stays fixed");
            require(writes==(scenario==0||scenario==3?2:1),"no write after earlier error");
            if(ok) {
                study_mesh::Triangle copy{}; std::memcpy(copy.data(),storage.data(),24);
                vertices[0].x=9;
                require(copy[0].x==0.25f,"data copied");
                require(buffer.replace_same_size(vertices.data(),3),"multiple replacements");
            }
            // After GL failure stop submitting; the owner is used only for cleanup.
        }
        require(deleted==1,"released once");
    }
    std::puts("same-size replacement: identity, copied data, no-op rejection, stage errors and cleanup passed");
}
