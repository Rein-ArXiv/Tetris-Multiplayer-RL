// Filled with the real source's WGL bootstrap and lookup bodies by the checker.
// This models return values on Linux, NOT Win32 ABI, a driver or native GUI.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#define WINAPI
using HDC=void*; using HGLRC=void*; using HMODULE=void*; using BOOL=int;
static int dc_tag,legacy_tag,core_tag,module_tag;
static HDC s_hdc=&dc_tag;
static HGLRC s_hglrc=nullptr;
static HMODULE s_opengl32=nullptr;
static bool s_should_close=false,s_frame_pacing=true;
static BOOL (*s_wglSwapInterval)(int)=nullptr;
static int mode,deleted_legacy,deleted_core,swaps;
static HGLRC wglCreateContext(HDC) { return mode==1 ? nullptr : &legacy_tag; }
static BOOL wglMakeCurrent(HDC,HGLRC c) {
    return !((mode==2 && c==&legacy_tag) || (mode==9 && c==&core_tag));
}
static BOOL wglDeleteContext(HGLRC c) {
    if(c==&legacy_tag) ++deleted_legacy;
    else if(c==&core_tag) ++deleted_core;
    else std::abort();
    return 1;
}
static HGLRC create_core(HDC,HGLRC,const int*) { return mode==8 ? nullptr : &core_tag; }
static BOOL swap_interval(int) { ++swaps; return 1; }
static void* invalid(int i) {
    const std::intptr_t values[]={0,1,2,3,-1};
    return reinterpret_cast<void*>(values[i]);
}
static void* wglGetProcAddress(const char* name) {
    if(std::strcmp(name,"wglCreateContextAttribsARB")==0)
        return mode>=3 && mode<=7 ? invalid(mode-3) : reinterpret_cast<void*>(create_core);
    if(std::strcmp(name,"wglSwapIntervalEXT")==0)
        return mode>=10 && mode<=14 ? invalid(mode-10) : reinterpret_cast<void*>(swap_interval);
    std::abort();
}
static HMODULE LoadLibraryA(const char*) { return &module_tag; }
static void* GetProcAddress(HMODULE,const char*) { return nullptr; }
/* LOOKUP_BODY */
static void bootstrap() {
/* BOOTSTRAP_BODY */
}
static void require(bool value) { if(!value) { std::fprintf(stderr,"WGL modeled mode=%d failed\n",mode); std::exit(1); } }
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    mode=std::atoi(argv[1]);
    bootstrap();
    const bool failure=mode>=1 && mode<=9;
    require(s_should_close==failure);
    require((s_hglrc==&core_tag)==!failure);
    require(deleted_legacy==(mode==1 ? 0 : 1));
    require(deleted_core==(mode==9 ? 1 : 0));
    require(swaps==(mode==0 ? 1 : 0));
}
