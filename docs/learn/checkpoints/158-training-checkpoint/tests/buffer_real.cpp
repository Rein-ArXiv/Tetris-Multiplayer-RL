// Non-presenting diagnostic. It does not change the window demo's double-buffer policy.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/vertex_buffer.h"
#include <cstdio>
using namespace study_gl;
static bool checks(const GlApi& gl) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major); gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    if (gl.GetError()!=0 || !(major>3 || (major==3 && minor>=3)) || !(profile & CoreProfileBit)) return false;
    const auto* driver=gl.GetString(Renderer);
    if (!driver || gl.GetError()!=0) return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(driver));
    VertexBuffer buffer(gl);
    {
        auto source=study_mesh::make_triangle();
        if (!buffer.upload(source)) return false;
        source[0].x=99; // A CPU edit cannot update the separate GL data store.
    }
    const auto expected=study_mesh::make_triangle();
    auto matches=[&]() {
        study_mesh::Triangle actual{};
        gl.GetBufferSubData(ArrayBuffer,0,static_cast<GLsizeiptr>(study_mesh::byte_count(actual)),actual.data());
        if (gl.GetError()!=0) return false;
        for (std::size_t i=0;i<actual.size();++i)
            if (actual[i].x!=expected[i].x || actual[i].y!=expected[i].y) return false;
        return true;
    };
    GLint size=0;
    gl.GetBufferParameteriv(ArrayBuffer,BufferSize,&size);
    if (gl.GetError()!=0 || size!=24 || !matches()) return false;
    std::puts("24 bytes read back after CPU edit and source destruction: matched");
    gl.BindBuffer(ArrayBuffer,0);
    GLint bound=-1; gl.GetIntegerv(ArrayBufferBinding,&bound);
    if (gl.GetError()!=0 || bound!=0) return false;
    gl.BufferData(ArrayBuffer,24,expected.data(),StaticDraw);
    if (gl.GetError()!=0x0502) return false; // No object bound: invalid operation.
    gl.BindBuffer(ArrayBuffer,buffer.name());
    if (gl.GetError()!=0 || !matches()) return false;
    std::puts("Unbind preserved storage; upload without binding was rejected");
    // Same name, newly defined smaller store; STATIC_DRAW does not prohibit this.
    const study_mesh::Vertex2 replacement[2]={{1,2},{3,4}};
    gl.BufferData(ArrayBuffer,sizeof(replacement),replacement,StaticDraw);
    gl.GetBufferParameteriv(ArrayBuffer,BufferSize,&size);
    study_mesh::Vertex2 actual[2]{};
    gl.GetBufferSubData(ArrayBuffer,0,sizeof(actual),actual);
    if (gl.GetError()!=0 || size!=16 || actual[0].x!=1 || actual[0].y!=2 ||
        actual[1].x!=3 || actual[1].y!=4) return false;
    std::puts("Same buffer name: data store replaced with 16 bytes and verified");
    buffer.reset(); buffer.reset();
    gl.GetIntegerv(ArrayBufferBinding,&bound);
    if (gl.GetError()!=0 || bound!=0 || buffer.name()!=0) return false;
    std::puts("Explicit reset and later destructor: no owned name remains");
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
        window=SDL_CreateWindow("buffer readback",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if (!window) break;
        context=SDL_GL_CreateContext(window);
        if (!context || SDL_GL_MakeCurrent(window,context)!=0) break;
        GlApi gl;
        if (!load(gl,SDL_GL_GetProcAddress)) break;
        result=checks(gl) ? 0 : 1; // All buffer owners die inside checks(), before context.
    } while(false);
    if (result) std::fprintf(stderr,"buffer probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); return result;
}
