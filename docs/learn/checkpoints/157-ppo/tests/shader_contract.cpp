#include "renderer/shader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
using namespace study_gl;
namespace {
int creates, sources, compiles, queries, logs, deletes, error_reads;
int api_failure; // 1=create, 2=source, 3=compile, 4=status, 5=log length, 6=log
bool zero_name, compile_ok, throw_log;
GLint supplied_length, forced_length;
GLenum pending;
std::string captured, supplied_log;
void require(bool ok,const char* why) {
    if (!ok) { std::fprintf(stderr,"Shader contract: %s\n",why); std::exit(1); }
}
void signal(int stage) { if (api_failure==stage) pending=0x0502; }
GLenum STUDY_GL_CALL error() { ++error_reads; auto e=pending; pending=0; return e; }
GLuint STUDY_GL_CALL create(GLenum kind) {
    ++creates; require(kind==VertexShader || kind==FragmentShader,"stage type");
    signal(1); return zero_name ? 0 : 41;
}
void STUDY_GL_CALL source(GLuint name,GLsizei count,const GLchar* const* strings,const GLint* lengths) {
    ++sources; require(name==41 && count==1 && strings && lengths,"explicit length input");
    supplied_length=*lengths; captured.assign(*strings,static_cast<std::size_t>(*lengths)); signal(2);
}
void STUDY_GL_CALL compile(GLuint name) { ++compiles; require(name==41,"compile handle"); signal(3); }
void STUDY_GL_CALL query(GLuint name,GLenum field,GLint* value) {
    ++queries; require(name==41,"query handle");
    if (field==CompileStatus) { *value=compile_ok ? 1 : 0; signal(4); }
    else {
        require(field==InfoLogLength,"log field");
        *value=forced_length>=0 ? forced_length : static_cast<GLint>(supplied_log.size()+1); signal(5);
    }
}
void STUDY_GL_CALL log(GLuint name,GLsizei capacity,GLsizei* written,GLchar* output) {
    ++logs; require(name==41 && capacity>0 && written,"log output");
    if (throw_log) throw std::runtime_error("injected diagnostic exception");
    require(static_cast<std::size_t>(capacity)==supplied_log.size()+1,"terminator capacity");
    std::memcpy(output,supplied_log.c_str(),supplied_log.size()+1);
    *written=static_cast<GLsizei>(supplied_log.size()); signal(6);
}
void STUDY_GL_CALL remove(GLuint name) { require(name==41,"delete own name"); ++deletes; }
GlApi table() {
    GlApi gl; gl.GetError=error; gl.CreateShader=create; gl.ShaderSource=source;
    gl.CompileShader=compile; gl.GetShaderiv=query; gl.GetShaderInfoLog=log; gl.DeleteShader=remove;
    return gl;
}
void clear() {
    creates=sources=compiles=queries=logs=deletes=error_reads=0;
    api_failure=0; zero_name=throw_log=false; compile_ok=true; pending=0;
    forced_length=-1; supplied_length=0; captured.clear(); supplied_log.clear();
}
}
int main() {
    auto gl=table(); clear();
    {
        Shader shader(gl);
        require(!shader.compile(0,"x") && !shader.diagnostic().empty() && error_reads==0,"invalid type diagnostic");
        require(!shader.compile(VertexShader,{}) && shader.diagnostic().find("empty")!=std::string::npos && error_reads==0,"empty source");
        const char bytes[3]={'a','b','c'}; // Not a terminated string.
        require(shader.compile(VertexShader,{bytes,3}) && captured=="abc" && supplied_length==3,"explicit source span");
        require(shader.name()==41 && shader.diagnostic().empty(),"success without log");
        require(!shader.compile(VertexShader,"replace") && creates==1 && shader.name()==41,"duplicate preserves owner");
        shader.reset(); shader.reset(); require(deletes==1,"idempotent reset");
        supplied_log=std::string(6000,'w');
        require(shader.compile(FragmentShader,"f") && shader.diagnostic().find(supplied_log)!=std::string::npos,"long success log is not failure");
    }
    require(deletes==2,"destructor cleanup");
    for (int length : {0,1}) {
        clear(); forced_length=length; compile_ok=false;
        Shader shader(gl);
        require(!shader.compile(VertexShader,"broken") && shader.name()==0 && deletes==1 &&
                shader.diagnostic().find("no compiler log")!=std::string::npos,"status failure without log");
    }
    clear(); compile_ok=false; supplied_log="line 3: syntax error";
    { Shader shader(gl); require(!shader.compile(VertexShader,"broken") &&
        shader.diagnostic().find(supplied_log)!=std::string::npos && deletes==1,"compiler failure cleanup"); }
    for (int fail=1;fail<=6;++fail) {
        clear(); api_failure=fail; supplied_log="a note";
        Shader shader(gl);
        require(!shader.compile(VertexShader,"text") && shader.name()==0 && deletes==1,"API failure cleanup");
        require(shader.diagnostic().find("API failure")!=std::string::npos,"API diagnosis distinct");
        if (fail==1) require(sources==0 && compiles==0,"creation failure stops source");
        if (fail==2) require(compiles==0,"source failure stops compile");
        if (fail<=3) require(queries==0,"early failure stops queries");
        if (fail<=5) require(logs==0,"failed query stops log read");
    }
    clear(); zero_name=true;
    { Shader shader(gl); require(!shader.compile(VertexShader,"text") && sources==0 && deletes==0,"zero creation short circuit"); }
    clear(); pending=0x0500;
    { Shader shader(gl); require(!shader.compile(VertexShader,"text") && creates==0,"pending error before allocation"); }
    clear(); throw_log=true; supplied_log="diagnostic";
    {
        Shader shader(gl); bool caught=false;
        try { shader.compile(VertexShader,"text"); } catch(const std::runtime_error&) { caught=true; }
        require(caught && shader.name()==0 && deletes==1,"exception cleanup then propagation");
    }
    std::puts("Shader contract: source length, status/log separation, failures, retry and exception cleanup passed");
}
