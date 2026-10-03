// Link to the teaching API or the production API; exercise real ownership code.
#ifdef LEARN_PRODUCTION
#include "../../platform/platform.h"
#else
#include "platform/platform.h"
#endif
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
static void require(bool okay, const char* what) {
    if (!okay) { std::fprintf(stderr,"GL contract: %s\n",what); std::exit(1); }
}
int main(int argc,char**) {
    const bool failure = argc > 1;
#ifdef LEARN_PRODUCTION
    platform_init(64,64,"production GL lifetime");
    require(platform_should_close() == failure,"production failure signal");
    platform_shutdown();
    require(SDL_WasInit(0) == 0,"SDL released");
#else
    for (int attempt=0;attempt<2;++attempt) {
        const bool ready = platform_init(64,64,"study GL lifetime");
        require(ready != failure,"init result");
        require(platform_should_close() == failure,"failure state unpublished");
        if (ready) {
            require(SDL_GL_GetCurrentContext() != nullptr,"current context");
            require(!platform_init(64,64,"duplicate"),"duplicate rejected");
            require(!platform_should_close(),"duplicate preserves session");
            platform_begin_frame(); platform_end_frame();
        }
        platform_shutdown(); platform_shutdown();
        require(platform_should_close(),"inactive after shutdown");
        require(SDL_GL_GetCurrentContext() == nullptr,"context released");
        require(SDL_WasInit(0) == 0,"SDL released");
    }
#endif
}
