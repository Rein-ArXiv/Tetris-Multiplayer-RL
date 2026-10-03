// Query actual vertex-array state without a shader, draw call or presentation.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/vertex_array.h"
#include "renderer/vertex_buffer.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
using namespace study_gl;

static bool attribute(const GlApi& gl, GLuint index, GLenum field, GLint expected) {
    GLint actual=-1;
    gl.GetVertexAttribiv(index,field,&actual);
    if (gl.GetError()!=0 || actual!=expected) {
        std::fprintf(stderr,"attribute %u field 0x%X: got %d expected %d\n",index,field,actual,expected);
        return false;
    }
    return true;
}
static bool checks(const GlApi& gl) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major); gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    if (gl.GetError()!=0 || !(major>3 || (major==3 && minor>=3)) || !(profile&CoreProfileBit)) return false;
    const auto* driver=gl.GetString(Renderer);
    if (!driver || gl.GetError()!=0) return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(driver));

    // Both buffers outlive both VAOs. GL names are not CPU pointers.
    VertexBuffer a(gl),b(gl);
    const auto data=study_mesh::make_triangle();
    if (!a.upload(data) || !b.upload(data)) return false;
    VertexArray first(gl),second(gl);
    if (!first.configure(a.name())) return false;
    if (!attribute(gl,0,AttribSize,2) || !attribute(gl,0,AttribType,Float) ||
        !attribute(gl,0,AttribNormalized,0) || !attribute(gl,0,AttribStride,8) ||
        !attribute(gl,0,AttribEnabled,1) || !attribute(gl,0,AttribBufferBinding,a.name())) return false;
    void* offset=nullptr;
    gl.GetVertexAttribPointerv(0,AttribPointer,&offset);
    if (gl.GetError()!=0 || offset!=nullptr) return false;
    std::puts("Position: size 2, FLOAT, normalized 0, stride 8, offset 0, enabled");

    gl.BindBuffer(ArrayBuffer,b.name());
    if (!attribute(gl,0,AttribBufferBinding,a.name())) return false;
    gl.BindBuffer(ArrayBuffer,0);
    if (!attribute(gl,0,AttribBufferBinding,a.name())) return false;
    std::puts("Bind B, then unbind ARRAY_BUFFER: first VAO still references A");

    // A second independent VAO captures B. Its enabled flag is also independent.
    if (!second.configure(b.name())) return false;
    gl.DisableVertexAttribArray(0);
    if (!attribute(gl,0,AttribEnabled,0)) return false;
    gl.BindVertexArray(first.name());
    if (!attribute(gl,0,AttribEnabled,1) || !attribute(gl,0,AttribBufferBinding,a.name())) return false;
    GLint bound=0;
    gl.GetIntegerv(ArrayBufferBinding,&bound);
    if (gl.GetError()!=0 || bound!=static_cast<GLint>(b.name())) return false;
    std::puts("Switch VAO: captured source/enabled restored, ARRAY_BUFFER binding stays B");

    // An observation-only second attribute reads y alone from the same Vertex2 array.
    gl.BindBuffer(ArrayBuffer,a.name());
    gl.VertexAttribPointer(1,1,Float,False,sizeof(study_mesh::Vertex2),
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offsetof(study_mesh::Vertex2,y))));
    gl.EnableVertexAttribArray(1);
    if (!attribute(gl,1,AttribSize,1) || !attribute(gl,1,AttribStride,8) ||
        !attribute(gl,1,AttribEnabled,1)) return false;
    gl.GetVertexAttribPointerv(1,AttribPointer,&offset);
    if (gl.GetError()!=0 || reinterpret_cast<std::uintptr_t>(offset)!=4) return false;
    gl.BindVertexArray(second.name());
    if (!attribute(gl,1,AttribEnabled,0) || !attribute(gl,0,AttribBufferBinding,b.name())) return false;
    std::puts("Y-only observation attribute: offset 4, stride 8, isolated to first VAO");

    // An API-legal stride can still describe the wrong layout; restore it before use.
    gl.BindVertexArray(first.name());
    gl.BindBuffer(ArrayBuffer,a.name());
    gl.VertexAttribPointer(0,2,Float,False,4,nullptr);
    if (!attribute(gl,0,AttribStride,4)) return false;
    gl.VertexAttribPointer(0,2,Float,False,8,nullptr);
    if (!attribute(gl,0,AttribStride,8)) return false;
    std::puts("Wrong stride 4 accepted as state; layout correctness needs its own check");

    gl.BindVertexArray(0);
    gl.GetIntegerv(VertexArrayBinding,&bound);
    if (gl.GetError()!=0 || bound!=0) return false;
    gl.VertexAttribPointer(0,2,Float,False,8,nullptr);
    const GLenum zero_error=gl.GetError();
    if (zero_error!=0x0502) {
        std::fprintf(stderr,"VAO zero VertexAttribPointer: error 0x%X, expected 0x0502\n",zero_error);
        return false;
    }
    first.reset(); first.reset(); second.reset();
    gl.GetIntegerv(VertexArrayBinding,&bound);
    if (gl.GetError()!=0 || bound!=0) return false;
    gl.BindBuffer(ArrayBuffer,a.name());
    GLint size=0; gl.GetBufferParameteriv(ArrayBuffer,BufferSize,&size);
    if (gl.GetError()!=0 || size!=24) return false;
    std::puts("VAO zero rejects configuration; VAO deletion leaves buffer A alive");
    return true;
}
int main() {
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
    if (SDL_Init(SDL_INIT_VIDEO)!=0) return 1;
    SDL_Window* window=nullptr; SDL_GLContext context=nullptr; int result=1;
    do {
        if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0) break;
        window=SDL_CreateWindow("VAO state",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if (!window) break;
        context=SDL_GL_CreateContext(window);
        if (!context || SDL_GL_MakeCurrent(window,context)!=0) break;
        GlApi gl;
        if (!load(gl,SDL_GL_GetProcAddress)) break;
        result=checks(gl) ? 0 : 1;
    } while(false);
    if (result) std::fprintf(stderr,"VAO probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); return result;
}
