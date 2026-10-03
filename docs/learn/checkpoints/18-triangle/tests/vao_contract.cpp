#include "renderer/vertex_array.h"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
using namespace study_gl;
namespace {
int fail_stage, stage, deletes, error_reads;
bool context_alive = true;
GLenum pending;
GLuint selected_array, selected_buffer;
void require(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr, "VAO contract: %s\n", reason); std::exit(1); }
}
void step(int expected) {
    require(context_alive && ++stage == expected, "call order/context");
    if (fail_stage == stage) pending = 0x0502;
}
GLenum STUDY_GL_CALL error() { ++error_reads; auto e=pending; pending=0; return e; }
void STUDY_GL_CALL gen(GLsizei count, GLuint* name) {
    step(1); require(count==1, "one name"); *name = fail_stage==6 ? 0 : 31;
}
void STUDY_GL_CALL bind_array(GLuint name) {
    step(2); require(name==31, "bind owned VAO"); selected_array=name;
}
void STUDY_GL_CALL bind_buffer(GLenum target, GLuint name) {
    step(3); require(target==ArrayBuffer && name==7, "borrow buffer 7"); selected_buffer=name;
}
void STUDY_GL_CALL pointer(GLuint index, GLint count, GLenum type, GLboolean normalized,
                           GLsizei stride, const void* offset) {
    step(4);
    require(selected_array==31 && selected_buffer==7, "capture selected objects");
    require(index==0 && count==2 && type==Float && normalized==False &&
            stride==8 && offset==nullptr, "position layout");
}
void STUDY_GL_CALL enable(GLuint index) { step(5); require(index==0, "enable location 0"); }
void STUDY_GL_CALL remove(GLsizei count, const GLuint* name) {
    require(context_alive && count==1 && *name==31, "delete own VAO while context lives");
    ++deletes;
}
GlApi table() {
    GlApi gl;
    gl.GetError=error; gl.GenVertexArrays=gen; gl.BindVertexArray=bind_array;
    gl.BindBuffer=bind_buffer; gl.VertexAttribPointer=pointer;
    gl.EnableVertexAttribArray=enable; gl.DeleteVertexArrays=remove;
    return gl; // DeleteBuffers is null: the VAO must never delete its input buffer.
}
void clear(int failure=0) {
    fail_stage=failure; stage=deletes=error_reads=0; pending=0;
    selected_array=selected_buffer=0;
}
}
static_assert(!std::is_copy_constructible<VertexArray>::value);
static_assert(!std::is_move_constructible<VertexArray>::value);
int main() {
    auto gl=table();
    clear();
    {
        VertexArray vao(gl);
        require(!vao.configure(0) && error_reads==0 && stage==0, "zero input does no GL work");
        pending=0x0500;
        require(!vao.configure(7) && stage==0 && deletes==0, "pending error aborts before allocation");
        require(vao.configure(7) && stage==5 && vao.name()==31, "all stages succeed");
        const int reads=error_reads;
        require(!vao.configure(7) && stage==5 && error_reads==reads && deletes==0,
                "duplicate preserves object");
        vao.reset(); vao.reset();
        require(vao.name()==0 && deletes==1, "idempotent reset");
        stage=0;
        require(vao.configure(7), "configure after reset");
    }
    require(deletes==2, "destructor cleans the new name");
    for (int failure=1; failure<=6; ++failure) {
        clear(failure);
        {
            VertexArray vao(gl);
            require(!vao.configure(7) && vao.name()==0, "failure leaves empty owner");
            require(stage==(failure==6 ? 1 : failure), "failure stops subsequent steps");
        }
        require(deletes==(failure==6 ? 0 : 1), "partial name cleaned exactly once");
    }
    context_alive=false;
    std::puts("VAO ownership: order, format, partial cleanup, retry, no buffer deletion passed");
}
