#include "gl_api.h"
#include "../platform/platform.h"

#include <cstdio>

#define GL_DEFINE(ret, name, args) ret (*gl_##name) args = nullptr;
GL_FUNCS(GL_DEFINE)
#undef GL_DEFINE

bool gl_load_functions()
{
    bool ok = true;

    // 조회 실패를 한 번에 모아 보여준다. 첫 실패에서 멈추면 드라이버가
    // 무엇을 얼마나 빠뜨렸는지 알 수 없어 원인 파악이 느려진다.
#define GL_LOAD(ret, name, args)                                               \
    gl_##name = (ret (*) args)platform_gl_get_proc("gl" #name);                \
    if (!gl_##name) {                                                          \
        std::fprintf(stderr, "[GL] missing entry point: gl%s\n", #name);       \
        ok = false;                                                            \
    }
    GL_FUNCS(GL_LOAD)
#undef GL_LOAD

    if (!ok) {
        std::fprintf(stderr,
                     "[GL] driver does not expose the OpenGL 3.3 Core entry "
                     "points this renderer needs.\n");
        return false;
    }

    const unsigned char* ver = gl_GetString(GL_VERSION);
    const unsigned char* ren = gl_GetString(GL_RENDERER);
    std::fprintf(stderr, "[GL] %s | %s\n",
                 ver ? (const char*)ver : "(unknown version)",
                 ren ? (const char*)ren : "(unknown renderer)");
    return true;
}
