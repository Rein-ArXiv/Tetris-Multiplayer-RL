// Observe source copying and compile status before introducing program linking.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include "renderer/shader.h"
#include "renderer/shader_sources.h"
#include <cstdio>
#include <exception>
#include <cstring>
using namespace study_gl;

static bool source_copy(const GlApi& gl) {
    const GLuint name=gl.CreateShader(VertexShader);
    if (gl.GetError()!=0 || !name) { if (name) gl.DeleteShader(name); return false; }
    bool ok=false;
    do {
        {
            char local[sizeof(study_shader::vertex)];
            std::memcpy(local,study_shader::vertex,sizeof local);
            const GLchar* text=local;
            const GLint length=sizeof(local)-1;
            gl.ShaderSource(name,1,&text,&length);
            if (gl.GetError()!=0) break;
            std::memset(local,'?',sizeof local); // Copy must not keep this CPU address.
        }
        gl.CompileShader(name);
        const GLenum api_error=gl.GetError();
        GLint compiled=0;
        gl.GetShaderiv(name,CompileStatus,&compiled);
        if (api_error!=0 || gl.GetError()!=0 || compiled!=1) break;
        std::puts("Source copy: CPU bytes overwritten and expired before successful compile");

        const GLchar* broken="#version 330 core\nvoid main() { gl_Position = ; }\n";
        gl.ShaderSource(name,1,&broken,nullptr);
        if (gl.GetError()!=0) break;
        gl.CompileShader(name);
        const GLenum syntax_api_error=gl.GetError();
        gl.GetShaderiv(name,CompileStatus,&compiled);
        if (syntax_api_error!=0 || gl.GetError()!=0 || compiled!=0) break;
        std::puts("Syntax error: COMPILE_STATUS false while GetError is zero");
        ok=true;
    } while(false);
    gl.DeleteShader(name); // No attachment: release this standalone observation object.
    return ok && gl.GetError()==0;
}
static bool checks(const GlApi& gl) {
    GLint major=0,minor=0,profile=0;
    gl.GetIntegerv(MajorVersion,&major); gl.GetIntegerv(MinorVersion,&minor);
    gl.GetIntegerv(ContextProfileMask,&profile);
    if (gl.GetError()!=0 || !(major>3 || (major==3 && minor>=3)) || !(profile&CoreProfileBit)) return false;
    const auto* driver=gl.GetString(Renderer);
    if (!driver || gl.GetError()!=0) return false;
    std::printf("Actual GL %d.%d Core | %s\n",major,minor,reinterpret_cast<const char*>(driver));
    if (!source_copy(gl)) return false;
    Shader vertex(gl),fragment(gl),bad(gl);
    if (!vertex.compile(VertexShader,study_shader::vertex) ||
        !fragment.compile(FragmentShader,study_shader::fragment)) return false;
    std::puts("Both GLSL 330 stages compile independently; no program link yet");
    if (bad.compile(VertexShader,"#version 330 core\nvoid main(){gl_Position = ;}\n") ||
        bad.name()!=0 || bad.diagnostic().empty()) return false;
    std::printf("Expected failure diagnostic: %s\n",bad.diagnostic().c_str());
    if (!bad.compile(VertexShader,study_shader::vertex)) return false;
    bad.reset(); bad.reset();
    if (bad.name()!=0 || gl.GetError()!=0) return false;
    std::puts("Failed object cleaned, diagnostic retained, corrected source retry succeeded");
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
        window=SDL_CreateWindow("shader compile",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if (!window) break;
        context=SDL_GL_CreateContext(window);
        if (!context || SDL_GL_MakeCurrent(window,context)!=0) break;
        GlApi gl;
        if (!load(gl,SDL_GL_GetProcAddress)) break;
        try { result=checks(gl) ? 0 : 1; }
        catch (const std::exception& e) { std::fprintf(stderr,"Shader probe exception: %s\n",e.what()); }
    } while(false);
    if (result) std::fprintf(stderr,"Shader probe failed; SDL diagnostic: %s\n",SDL_GetError());
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); return result;
}
