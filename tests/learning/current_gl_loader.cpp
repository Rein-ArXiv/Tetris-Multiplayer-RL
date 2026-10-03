// Real production loader, typed stubs only. No OS or GL context exercised.
#include "../../renderer/gl_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> names;
static std::string missing;
static unsigned query_calls;
template<class T> static T result() { return T{}; }
template<> void result<void>() {}
#define STUB(ret,name,args) static ret TETRIS_GL_APIENTRY fake_##name args { return result<ret>(); }
GL_FUNCS(STUB)
#undef STUB
static const unsigned char* TETRIS_GL_APIENTRY report(GLenum) {
    ++query_calls; return reinterpret_cast<const unsigned char*>("typed test driver");
}
void* platform_gl_get_proc(const char* name) {
    names.emplace_back(name);
    if (missing == name || missing == "all") return nullptr;
    if (std::strcmp(name,"glGetString")==0) return reinterpret_cast<void*>(report);
#define RESOLVE(ret,n,args) if (std::strcmp(name,"gl" #n)==0) return reinterpret_cast<void*>(fake_##n);
    GL_FUNCS(RESOLVE)
#undef RESOLVE
    std::abort();
}
static void require(bool okay,const char* message) {
    if (!okay) { std::fprintf(stderr,"production loader: %s\n",message); std::exit(1); }
}
int main() {
    require(gl_load_functions(),"full lookup");
    const auto full=names;
    require(query_calls==2,"diagnostics on success");
    for (const auto& entry:full) {
        missing=entry; names.clear(); query_calls=0;
        require(!gl_load_functions(),"missing detected");
        require(names==full && query_calls==0,"all names checked, no GL calls on failure");
    }
    missing="all"; names.clear(); query_calls=0;
    require(!gl_load_functions() && names==full && query_calls==0,"all missing");
    missing.clear(); names.clear();
    require(gl_load_functions() && names==full && query_calls==2,"reload");
    std::printf("Production loader: %zu symbols individually absent, all absent and reload passed\n",full.size());
}
