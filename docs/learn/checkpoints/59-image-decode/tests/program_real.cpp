// Observe separate compilation, interface mismatch, detachment and selection.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/shader.h"
#include "renderer/shader_sources.h"
#include "renderer/program.h"
#include <cstdio>
#include <exception>
using namespace study_gl;
static constexpr char mismatch[] = R"glsl(#version 330 core
in vec2 v_color;
out vec4 out_color;
void main() { out_color = vec4(v_color, 0.0, 1.0); }
)glsl";
// Declared after owners so every early return unbinds before their destruction.
struct Unbind { const GlApi& gl; ~Unbind() { gl.UseProgram(0); } };
static bool checks(const GlApi& gl) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major); gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    if (gl.GetError()!=0 || !(major>3 || (major==3 && minor>=3)) || !(profile&CoreProfileBit)) return false;
    const auto* driver=gl.GetString(Renderer);
    if (!driver || gl.GetError()!=0) return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(driver));
    Shader vertex(gl),fragment(gl),bad(gl);
    if (!vertex.compile(VertexShader,study_shader::vertex) ||
        !fragment.compile(FragmentShader,study_shader::fragment) ||
        !bad.compile(FragmentShader,mismatch)) return false;
    std::puts("Both matching and mismatched stages compile independently");
    const GLuint raw=gl.CreateProgram();
    if (!raw) return false;
    gl.AttachShader(raw,vertex.name()); gl.AttachShader(raw,bad.name());
    gl.LinkProgram(raw); const GLenum api_error=gl.GetError();
    GLint linked=1; gl.GetProgramiv(raw,LinkStatus,&linked);
    const GLenum query_error=gl.GetError();
    gl.DeleteProgram(raw);
    if (api_error || query_error || linked!=0 || gl.GetError()) return false;
    std::puts("Consumed vec3/vec2 mismatch: LINK_STATUS false while GetError is zero");
    Program a(gl),b(gl);
    Unbind unbind{gl};
    if (a.link(vertex.name(),bad.name()) || a.name()!=0 || a.diagnostic().empty()) return false;
    std::printf("Expected link diagnostic: %s\n",a.diagnostic().c_str());
    if (!a.link(vertex.name(),fragment.name())) return false;
    GLint current=-1; gl.GetIntegerv(CurrentProgram,&current);
    if (gl.GetError() || current!=0) return false;
    gl.UseProgram(a.name());
    if (gl.GetError() || !b.link(vertex.name(),fragment.name())) return false;
    gl.GetIntegerv(CurrentProgram,&current);
    if (gl.GetError() || static_cast<GLuint>(current)!=a.name()) return false;
    std::puts("Linking a new program preserves the currently selected program");
    GLint attached=-1; gl.GetProgramiv(b.name(),AttachedShaders,&attached);
    if (gl.GetError() || attached!=0) return false;
    vertex.reset(); fragment.reset(); bad.reset();
    gl.GetProgramiv(b.name(),LinkStatus,&linked);
    gl.UseProgram(b.name()); gl.GetIntegerv(CurrentProgram,&current);
    if (gl.GetError() || linked!=1 || static_cast<GLuint>(current)!=b.name()) return false;
    std::puts("Detached shaders released; linked program remains usable with zero attachments");
    gl.UseProgram(0); b.reset(); b.reset(); a.reset();
    gl.GetIntegerv(CurrentProgram,&current);
    if (gl.GetError() || current!=0) return false;
    std::puts("Selection cleared before idempotent program deletion; no draw performed");
    return true;
}
int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO)!=0) return 1;
    SDL_Window* window=nullptr; SDL_GLContext context=nullptr; int result=1;
    do {
        if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)!=0 ||
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,0)!=0) break;
        window=SDL_CreateWindow("program link",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if (!window) break;
        context=SDL_GL_CreateContext(window);
        if (!context || SDL_GL_MakeCurrent(window,context)!=0) break;
        GlApi gl;
        if (!load(gl,SDL_GL_GetProcAddress)) break;
        try { result=checks(gl) ? 0 : 1; }
        catch (const std::exception& e) { std::fprintf(stderr,"Program probe exception: %s\n",e.what()); }
    } while(false);
    if (result) std::fprintf(stderr,"Program probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); return result;
}
