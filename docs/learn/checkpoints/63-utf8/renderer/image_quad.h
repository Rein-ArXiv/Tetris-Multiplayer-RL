#pragma once
#include "renderer/image_geometry.h"
#include "renderer/texture.h"
#include "renderer/program.h"
#include "renderer/shader.h"
namespace study_image_quad {
inline constexpr char vertex_shader[]=R"glsl(#version 330 core
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_tint;
out vec2 v_uv;
out vec4 v_tint;
void main(){gl_Position=vec4(a_position,0,1);v_uv=a_uv;v_tint=a_tint;}
)glsl";
inline constexpr char fragment_shader[]=R"glsl(#version 330 core
in vec2 v_uv;
in vec4 v_tint;
uniform sampler2D u_image;
layout(location=0) out vec4 out_color;
void main(){out_color=texture(u_image,v_uv)*v_tint;}
)glsl";
// Exclusive image pass: unit 0 without sampler objects; caller sets viewport/clip,
// disables depth/stencil/cull/dither/framebuffer-sRGB and uses default write masks.
// Normal exit clears VAO/VBO/program/unit0 binding and disables blend. This is
// a known pass boundary, not arbitrary caller-state preservation.
class ImageQuad {
public:
    explicit ImageQuad(const study_gl::GlApi& gl) noexcept : gl_(gl),program_(gl) {}
    ~ImageQuad() noexcept { reset(); }
    ImageQuad(const ImageQuad&)=delete;
    ImageQuad& operator=(const ImageQuad&)=delete;
    bool init() {
        using namespace study_gl;
        if(ready_ || gl_.GetError()) return false;
        Shader vs(gl_),fs(gl_);
        if(!vs.compile(VertexShader,vertex_shader)||!fs.compile(FragmentShader,fragment_shader)||
           !program_.link(vs.name(),fs.name())) {reset();return false;}
        location_=gl_.GetUniformLocation(program_.name(),"u_image");
        gl_.GenBuffers(1,&buffer_);gl_.GenVertexArrays(1,&array_);
        if(location_<0 || !buffer_ || !array_ || gl_.GetError()){reset();return false;}
        gl_.BindVertexArray(array_);bool ok=gl_.GetError()==0;
        if(ok){gl_.BindBuffer(ArrayBuffer,buffer_);ok=gl_.GetError()==0;}
        if(ok){gl_.BufferData(ArrayBuffer,6*sizeof(Vertex),nullptr,DynamicDraw);ok=gl_.GetError()==0;}
        if(ok){
            gl_.VertexAttribPointer(0,2,Float,False,sizeof(Vertex),nullptr);
            gl_.EnableVertexAttribArray(0);
            gl_.VertexAttribPointer(1,2,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,u)));
            gl_.EnableVertexAttribArray(1);
            gl_.VertexAttribPointer(2,4,Float,False,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,r)));
            gl_.EnableVertexAttribArray(2);
            ok=gl_.GetError()==0;
        }
        gl_.BindVertexArray(0);gl_.BindBuffer(ArrayBuffer,0);
        const bool clean=gl_.GetError()==0;
        if(!ok || !clean){reset();return false;}
        ready_=true;return true;
    }
    bool draw(const study_texture::Texture& image,const Draw& request) noexcept {
        const auto vertices=make_vertices(request);
        if(!vertices || !ready_ || !image.name()) return false;
        using namespace study_gl;
        if(gl_.GetError())return false;
        gl_.BindVertexArray(array_);bool ok=gl_.GetError()==0;
        if(ok){gl_.BindBuffer(ArrayBuffer,buffer_);ok=gl_.GetError()==0;}
        if(ok){gl_.BufferData(ArrayBuffer,sizeof(*vertices),vertices->data(),DynamicDraw);ok=gl_.GetError()==0;}
        if(ok){
            gl_.ActiveTexture(Texture0);gl_.BindTexture(Texture2D,image.name());
            gl_.UseProgram(program_.name());gl_.Uniform1i(location_,0);
            gl_.BlendEquation(FuncAdd);
            gl_.BlendFuncSeparate(SrcAlpha,OneMinusSrcAlpha,One,OneMinusSrcAlpha);
            gl_.Enable(Blend);ok=gl_.GetError()==0;
        }
        if(ok){gl_.DrawArrays(Triangles,0,6);ok=gl_.GetError()==0;}
        gl_.BindVertexArray(0);gl_.BindBuffer(ArrayBuffer,0);
        gl_.UseProgram(0);gl_.ActiveTexture(Texture0);gl_.BindTexture(Texture2D,0);gl_.Disable(Blend);
        const bool clean=gl_.GetError()==0;
        return ok && clean;
    }
private:
    void reset() noexcept {
        if(array_)gl_.DeleteVertexArrays(1,&array_);
        if(buffer_)gl_.DeleteBuffers(1,&buffer_);
        array_=buffer_=0;ready_=false;program_.reset();
    }
    const study_gl::GlApi& gl_;
    study_gl::Program program_;
    study_gl::GLuint buffer_=0,array_=0;
    study_gl::GLint location_=-1;
    bool ready_=false;
};
} // namespace study_image_quad
