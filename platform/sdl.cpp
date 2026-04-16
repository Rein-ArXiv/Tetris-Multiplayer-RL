// platform/sdl.cpp — SDL2 + OpenGL 백엔드 (Mac/Linux/Windows 크로스플랫폼)
//
// platform/win32.cpp 와 동일한 10개 함수를 SDL2 로 재구현.
// 대응:
//   CreateWindowEx + wglCreateContext   → SDL_CreateWindow + SDL_GL_CreateContext
//   PeekMessage/DispatchMessage         → SDL_PollEvent
//   SwapBuffers(hdc)                    → SDL_GL_SwapWindow(win)
//   QueryPerformanceCounter             → SDL_GetPerformanceCounter
//   keyState[VK_*]                      → SDL_Event → VK 매핑 후 동일 테이블
//   WM_CHAR                             → SDL_TEXTINPUT
//
// 주의: main.cpp 와 game.cpp 는 수정하지 않는다. PlatformKey 값(Win32 VK)은 유지.

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include "platform.h"
#include "gl_defs.h"

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl.h>
#else
  #include <GL/gl.h>
#endif

// ─── GL 함수 포인터 (renderer.cpp 에서 extern 으로 참조) ──────────────────────
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
typedef void   (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void   (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void   (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
typedef GLint  (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void   (APIENTRY *PFNGLUNIFORM4FPROC)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void   (APIENTRY *PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void   (APIENTRY *PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void   (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void   (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void   (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void   (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void   (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void   (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);

PFNGLCREATESHADERPROC            glCreateShader            = nullptr;
PFNGLSHADERSOURCEPROC            glShaderSource            = nullptr;
PFNGLCOMPILESHADERPROC           glCompileShader           = nullptr;
PFNGLCREATEPROGRAMPROC           glCreateProgram           = nullptr;
PFNGLATTACHSHADERPROC            glAttachShader            = nullptr;
PFNGLLINKPROGRAMPROC             glLinkProgram             = nullptr;
PFNGLUSEPROGRAMPROC              glUseProgram              = nullptr;
PFNGLDELETESHADERPROC            glDeleteShader            = nullptr;
PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation      = nullptr;
PFNGLUNIFORM4FPROC               glUniform4f               = nullptr;
PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv        = nullptr;
PFNGLUNIFORM1IPROC               glUniform1i               = nullptr;
PFNGLUNIFORM1FPROC               glUniform1f               = nullptr;
PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays         = nullptr;
PFNGLBINDVERTEXARRAYPROC         glBindVertexArray         = nullptr;
PFNGLGENBUFFERSPROC              glGenBuffers              = nullptr;
PFNGLBINDBUFFERPROC              glBindBuffer              = nullptr;
PFNGLBUFFERDATAPROC              glBufferData              = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer     = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLACTIVETEXTUREPROC           glActiveTexture           = nullptr;
PFNGLGETSHADERIVPROC             glGetShaderiv             = nullptr;
PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog        = nullptr;
PFNGLGETPROGRAMIVPROC            glGetProgramiv            = nullptr;
PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog       = nullptr;
PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays      = nullptr;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers           = nullptr;
PFNGLDELETEPROGRAMPROC           glDeleteProgram           = nullptr;

// text_win32 는 wglUseFontBitmaps 용 glWindowPos2i 를 쓰지만, SDL 빌드에서는
// text_stb 가 대체하므로 glWindowPos2i 는 참조되지 않는다. 아무 값 유지.

#define LOAD_GL(type, name) \
    name = (type)SDL_GL_GetProcAddress(#name); \
    if (!name) fprintf(stderr, "[GL] SDL_GL_GetProcAddress failed: " #name "\n");

static void gl_load_functions()
{
    LOAD_GL(PFNGLCREATESHADERPROC,            glCreateShader)
    LOAD_GL(PFNGLSHADERSOURCEPROC,            glShaderSource)
    LOAD_GL(PFNGLCOMPILESHADERPROC,           glCompileShader)
    LOAD_GL(PFNGLCREATEPROGRAMPROC,           glCreateProgram)
    LOAD_GL(PFNGLATTACHSHADERPROC,            glAttachShader)
    LOAD_GL(PFNGLLINKPROGRAMPROC,             glLinkProgram)
    LOAD_GL(PFNGLUSEPROGRAMPROC,              glUseProgram)
    LOAD_GL(PFNGLDELETESHADERPROC,            glDeleteShader)
    LOAD_GL(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation)
    LOAD_GL(PFNGLUNIFORM4FPROC,               glUniform4f)
    LOAD_GL(PFNGLUNIFORMMATRIX4FVPROC,        glUniformMatrix4fv)
    LOAD_GL(PFNGLUNIFORM1IPROC,               glUniform1i)
    LOAD_GL(PFNGLUNIFORM1FPROC,               glUniform1f)
    LOAD_GL(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays)
    LOAD_GL(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray)
    LOAD_GL(PFNGLGENBUFFERSPROC,              glGenBuffers)
    LOAD_GL(PFNGLBINDBUFFERPROC,              glBindBuffer)
    LOAD_GL(PFNGLBUFFERDATAPROC,              glBufferData)
    LOAD_GL(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer)
    LOAD_GL(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)
    LOAD_GL(PFNGLACTIVETEXTUREPROC,           glActiveTexture)
    LOAD_GL(PFNGLGETSHADERIVPROC,             glGetShaderiv)
    LOAD_GL(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog)
    LOAD_GL(PFNGLGETPROGRAMIVPROC,            glGetProgramiv)
    LOAD_GL(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog)
    LOAD_GL(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays)
    LOAD_GL(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers)
    LOAD_GL(PFNGLDELETEPROGRAMPROC,           glDeleteProgram)
}

// ─── 상태 ─────────────────────────────────────────────────────────────────────
static SDL_Window*   s_win           = nullptr;
static SDL_GLContext s_glctx         = nullptr;
static bool          s_should_close  = false;
static int           s_win_w         = 0;
static int           s_win_h         = 0;

static bool s_key_state[256] = {};
static bool s_key_prev [256] = {};

static char s_char_queue[64] = {};
static int  s_char_head = 0;
static int  s_char_tail = 0;

static uint64_t s_freq        = 1;
static uint64_t s_init_time   = 0;
static uint64_t s_frame_start = 0;

// ─── SDL_Keycode → PlatformKey(=Win32 VK) 매핑 ──────────────────────────────
static int sdl_to_vk(SDL_Keycode k)
{
    switch (k) {
    case SDLK_LEFT:     return PKEY_LEFT;
    case SDLK_RIGHT:    return PKEY_RIGHT;
    case SDLK_UP:       return PKEY_UP;
    case SDLK_DOWN:     return PKEY_DOWN;
    case SDLK_SPACE:    return PKEY_SPACE;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return PKEY_ENTER;
    case SDLK_ESCAPE:   return PKEY_ESCAPE;
    case SDLK_BACKSPACE:return PKEY_BACK;
    case SDLK_q:        return PKEY_Q;
    case SDLK_r:        return PKEY_R;
    case SDLK_h:        return PKEY_H;
    case SDLK_p:        return PKEY_P;
    case SDLK_F5:       return PKEY_F5;
    case SDLK_F6:       return PKEY_F6;
    default: return -1;
    }
}

void platform_init(int w, int h, const char* title)
{
    s_win_w = w; s_win_h = h;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[SDL] SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);  // macOS 최소 지원 코어
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    s_win = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!s_win) {
        fprintf(stderr, "[SDL] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    s_glctx = SDL_GL_CreateContext(s_win);
    if (!s_glctx) {
        fprintf(stderr, "[SDL] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return;
    }
    SDL_GL_MakeCurrent(s_win, s_glctx);
    SDL_GL_SetSwapInterval(1);  // vsync

    gl_load_functions();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SDL_StartTextInput();

    s_freq        = SDL_GetPerformanceFrequency();
    s_init_time   = SDL_GetPerformanceCounter();
    s_frame_start = s_init_time;
}

void platform_shutdown()
{
    if (s_glctx) { SDL_GL_DeleteContext(s_glctx); s_glctx = nullptr; }
    if (s_win)   { SDL_DestroyWindow(s_win);      s_win   = nullptr; }
    SDL_Quit();
}

bool platform_should_close() { return s_should_close; }

float platform_begin_frame()
{
    memcpy(s_key_prev, s_key_state, sizeof(s_key_state));

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            s_should_close = true; break;
        case SDL_KEYDOWN: {
            int vk = sdl_to_vk(ev.key.keysym.sym);
            if (vk >= 0 && vk < 256) s_key_state[vk] = true;
            if (ev.key.keysym.sym == SDLK_ESCAPE) s_should_close = true;
        } break;
        case SDL_KEYUP: {
            int vk = sdl_to_vk(ev.key.keysym.sym);
            if (vk >= 0 && vk < 256) s_key_state[vk] = false;
        } break;
        case SDL_TEXTINPUT: {
            for (const char* p = ev.text.text; *p; ++p) {
                unsigned c = (unsigned char)*p;
                if (c < 128) {
                    int next = (s_char_tail + 1) % 64;
                    if (next != s_char_head) {
                        s_char_queue[s_char_tail] = (char)c;
                        s_char_tail = next;
                    }
                }
            }
        } break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                s_win_w = ev.window.data1;
                s_win_h = ev.window.data2;
                glViewport(0, 0, s_win_w, s_win_h);
            }
            break;
        }
    }

    uint64_t now = SDL_GetPerformanceCounter();
    float dt = (float)(now - s_frame_start) / (float)s_freq;
    s_frame_start = now;
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

void platform_end_frame()
{
    SDL_GL_SwapWindow(s_win);
}

bool platform_key_pressed(int key)
{
    if (key < 0 || key >= 256) return false;
    return s_key_state[key] && !s_key_prev[key];
}

bool platform_key_down(int key)
{
    if (key < 0 || key >= 256) return false;
    return s_key_state[key];
}

char platform_get_char_pressed()
{
    if (s_char_head == s_char_tail) return 0;
    char c = s_char_queue[s_char_head];
    s_char_head = (s_char_head + 1) % 64;
    return c;
}

double platform_get_time()
{
    uint64_t now = SDL_GetPerformanceCounter();
    return (double)(now - s_init_time) / (double)s_freq;
}

void* platform_get_hdc()
{
    // SDL 빌드에서는 text_stb 가 이 값을 사용하지 않음.
    return nullptr;
}
