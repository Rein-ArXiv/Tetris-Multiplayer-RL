// platform/win32.cpp — Win32 윈도우 + OpenGL 컨텍스트 + 입력 + 타이머
//
// 학습 포인트: raylib의 InitWindow() 는 이 파일 전체를 숨겨놓은 것입니다.
// raylib/rcore.c 를 열면 이와 동일한 Win32 코드를 볼 수 있습니다.
//
// 핵심 개념:
//   1. RegisterClassEx / CreateWindowEx  → OS가 창을 만드는 방법
//   2. PIXELFORMATDESCRIPTOR             → 이 창에서 OpenGL을 쓸 것임을 OS에 알림
//   3. wglCreateContext                  → OpenGL 렌더링 컨텍스트 생성
//   4. WM_KEYDOWN/UP 메시지 처리         → 입력 상태 테이블 유지
//   5. QueryPerformanceCounter           → 마이크로초 정밀도 타이머
//   6. PeekMessage + DispatchMessage     → 논블로킹 메시지 루프

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cstring>
#include <cstdio>
#include "platform.h"

// ─────────────────────────────────────────────────────────────────────────────
// GL 2.0+ 함수 포인터 타입 정의
// opengl32.lib 는 OpenGL 1.1 만 직접 export 합니다.
// 2.0 이후 함수는 wglGetProcAddress() 로 런타임에 불러와야 합니다.
// ─────────────────────────────────────────────────────────────────────────────
#include "gl_defs.h"  // GLchar, GLsizeiptr, GL_ARRAY_BUFFER 등 (renderer.cpp 와 공유)

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
typedef void   (APIENTRY *PFNGLWINDOWPOS2IPROC)(GLint, GLint);
typedef void   (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);

// ─── 전역 GL 함수 포인터 (renderer.cpp 에서도 extern 으로 선언해 사용) ──────
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
PFNGLWINDOWPOS2IPROC             glWindowPos2i             = nullptr;
PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays      = nullptr;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers           = nullptr;
PFNGLDELETEPROGRAMPROC           glDeleteProgram           = nullptr;

#define LOAD_GL(type, name) \
    name = (type)wglGetProcAddress(#name); \
    if (!name) fprintf(stderr, "[GL] wglGetProcAddress failed: " #name "\n");

static bool s_gl_load_ok = true;
// 치명적 함수 전용: 로드 실패 시 s_gl_load_ok = false (렌더링 불가)
#define LOAD_GL_CRITICAL(type, name) \
    name = (type)wglGetProcAddress(#name); \
    if (!name) { fprintf(stderr, "[GL] CRITICAL: " #name " not available\n"); s_gl_load_ok = false; }

static void gl_load_functions()
{
    s_gl_load_ok = true;
    // 렌더링에 필수인 함수들 — 하나라도 실패하면 s_gl_load_ok = false
    LOAD_GL_CRITICAL(PFNGLCREATESHADERPROC,            glCreateShader)
    LOAD_GL_CRITICAL(PFNGLSHADERSOURCEPROC,            glShaderSource)
    LOAD_GL_CRITICAL(PFNGLCOMPILESHADERPROC,           glCompileShader)
    LOAD_GL_CRITICAL(PFNGLCREATEPROGRAMPROC,           glCreateProgram)
    LOAD_GL_CRITICAL(PFNGLATTACHSHADERPROC,            glAttachShader)
    LOAD_GL_CRITICAL(PFNGLLINKPROGRAMPROC,             glLinkProgram)
    LOAD_GL_CRITICAL(PFNGLUSEPROGRAMPROC,              glUseProgram)
    LOAD_GL_CRITICAL(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation)
    LOAD_GL_CRITICAL(PFNGLUNIFORM4FPROC,               glUniform4f)
    LOAD_GL_CRITICAL(PFNGLUNIFORMMATRIX4FVPROC,        glUniformMatrix4fv)
    LOAD_GL_CRITICAL(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays)
    LOAD_GL_CRITICAL(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray)
    LOAD_GL_CRITICAL(PFNGLGENBUFFERSPROC,              glGenBuffers)
    LOAD_GL_CRITICAL(PFNGLBINDBUFFERPROC,              glBindBuffer)
    LOAD_GL_CRITICAL(PFNGLBUFFERDATAPROC,              glBufferData)
    LOAD_GL_CRITICAL(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer)
    LOAD_GL_CRITICAL(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)
    // 비필수 (cleanup/debug/optional) — 실패해도 계속
    LOAD_GL(PFNGLDELETESHADERPROC,            glDeleteShader)
    LOAD_GL(PFNGLUNIFORM1IPROC,               glUniform1i)
    LOAD_GL(PFNGLUNIFORM1FPROC,               glUniform1f)
    LOAD_GL(PFNGLACTIVETEXTUREPROC,           glActiveTexture)
    LOAD_GL(PFNGLGETSHADERIVPROC,             glGetShaderiv)
    LOAD_GL(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog)
    LOAD_GL(PFNGLGETPROGRAMIVPROC,            glGetProgramiv)
    LOAD_GL(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog)
    LOAD_GL(PFNGLWINDOWPOS2IPROC,             glWindowPos2i)
    LOAD_GL(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays)
    LOAD_GL(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers)
    LOAD_GL(PFNGLDELETEPROGRAMPROC,           glDeleteProgram)

    if (!s_gl_load_ok)
    {
        fprintf(stderr, "[GL] FATAL: Critical GL functions unavailable. "
                        "GPU driver may not support OpenGL 2.0+.\n");
    }
}

// ─── 정적 상태 ────────────────────────────────────────────────────────────────
static HWND   s_hwnd         = nullptr;
static HDC    s_hdc          = nullptr;
static HGLRC  s_hglrc        = nullptr;
static bool   s_should_close = false;
static int    s_win_w        = 0;
static int    s_win_h        = 0;

// 키 상태 테이블: WM_KEYDOWN 에서 true, WM_KEYUP 에서 false
static bool   s_key_state[256] = {};
// 직전 프레임의 키 상태 스냅샷 — IsKeyPressed 구현에 사용
static bool   s_key_prev[256]  = {};

// WM_CHAR 로 들어온 문자 큐 (원형 버퍼)
static char   s_char_queue[64] = {};
static int    s_char_head = 0;
static int    s_char_tail = 0;

// 타이머
static LARGE_INTEGER s_freq;
static LARGE_INTEGER s_frame_start;
static LARGE_INTEGER s_init_time;

// ─── 윈도우 프로시저 (WndProc) ────────────────────────────────────────────────
// OS 로부터 윈도우 메시지를 받는 콜백 함수.
// 모든 키 입력, 윈도우 크기 변경, 창 닫기 이벤트를 여기서 처리합니다.
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // wParam 은 VK_* 코드 (0–255). PlatformKey 값과 직접 대응.
        if (wParam < 256) s_key_state[wParam] = true;
        if (wParam == VK_ESCAPE) s_should_close = true;
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < 256) s_key_state[wParam] = false;
        return 0;

    case WM_CHAR:
        // 텍스트 입력 문자 (ConnectInput 화면에서 IP 주소 입력 등)
        if (wParam > 0 && wParam < 128)
        {
            int next = (s_char_tail + 1) % 64;
            if (next != s_char_head)
            {
                s_char_queue[s_char_tail] = (char)wParam;
                s_char_tail = next;
            }
        }
        return 0;

    case WM_SIZE:
        s_win_w = LOWORD(lParam);
        s_win_h = HIWORD(lParam);
        if (s_hglrc) glViewport(0, 0, s_win_w, s_win_h);
        return 0;

    case WM_CLOSE:
        s_should_close = true;
        return 0;

    case WM_DESTROY:
        s_should_close = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ─── platform_init ────────────────────────────────────────────────────────────
// raylib::InitWindow() 의 속을 드러낸 버전입니다.
// 아래 단계가 그대로 raylib 내부에서 일어납니다.
void platform_init(int w, int h, const char* title)
{
    s_win_w = w;
    s_win_h = h;

    // 타이머 초기화: QueryPerformanceFrequency 는 CPU 클럭 주파수를 반환
    QueryPerformanceFrequency(&s_freq);
    QueryPerformanceCounter(&s_init_time);
    s_frame_start = s_init_time;

    // ── 1. 윈도우 클래스 등록 ──────────────────────────────────────────────
    // OS 에 "이런 속성의 창 종류를 만들겠다"고 등록.
    // CS_OWNDC: 이 창 전용 DC 를 유지 (OpenGL에 필수)
    WNDCLASSEXA wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "TetrisHandmade";
    RegisterClassExA(&wc);

    // ── 2. 창 생성 ────────────────────────────────────────────────────────
    // AdjustWindowRect: 클라이언트 영역(게임 화면)이 정확히 w×h 가 되도록
    //                   타이틀바/테두리를 포함한 전체 창 크기를 계산
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT  rect  = {0, 0, w, h};
    AdjustWindowRect(&rect, style, FALSE);

    s_hwnd = CreateWindowExA(
        0, "TetrisHandmade", title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, wc.hInstance, nullptr);

    // ── 3. Device Context (DC) 획득 ────────────────────────────────────────
    // DC = 이 창의 "그림판 핸들". GDI와 OpenGL 모두 DC를 통해 창에 그림.
    s_hdc = GetDC(s_hwnd);

    // ── 4. OpenGL 픽셀 포맷 설정 ──────────────────────────────────────────
    // OS에 "이 창은 OpenGL을 쓸 것이고, 더블 버퍼링, 32비트 색상이 필요"하다고 알림.
    // SetPixelFormat은 창당 한 번만 호출 가능.
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(s_hdc, &pfd);
    SetPixelFormat(s_hdc, fmt, &pfd);

    // ── 5. OpenGL 렌더링 컨텍스트 생성 ────────────────────────────────────
    // wglCreateContext: GPU 드라이버에 "이 DC용 OpenGL 컨텍스트를 만들어라" 요청
    // 컨텍스트 = GPU 상태 머신의 인스턴스 (바인딩된 버퍼, 셰이더, 텍스처 등)
    // wglMakeCurrent: 이 스레드의 현재 GL 컨텍스트로 설정
    s_hglrc = wglCreateContext(s_hdc);
    wglMakeCurrent(s_hdc, s_hglrc);

    // ── 6. OpenGL 2.0+ 함수 포인터 로드 ──────────────────────────────────
    // opengl32.lib는 1.1만 노출. 나머지는 GPU 드라이버 DLL에서 직접 로드.
    gl_load_functions();

    // ── 7. 블렌딩 활성화 (투명도 지원) ────────────────────────────────────
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
}

void platform_shutdown()
{
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(s_hglrc);
    ReleaseDC(s_hwnd, s_hdc);
    DestroyWindow(s_hwnd);
}

bool platform_should_close()
{
    return s_should_close;
}

// ─── platform_begin_frame ─────────────────────────────────────────────────────
// 1. 이전 프레임 키 상태 저장 (IsKeyPressed 구현을 위해)
// 2. PeekMessage 루프로 OS 메시지를 모두 처리 (입력, 창 이벤트 등)
// 3. 델타타임 계산 후 반환
float platform_begin_frame()
{
    // 이전 프레임 키 상태 스냅샷 → platform_key_pressed 구현에 사용
    memcpy(s_key_prev, s_key_state, sizeof(s_key_state));

    // PeekMessage: 메시지가 있으면 처리하고 없으면 즉시 반환 (논블로킹)
    // GetMessage와 달리 메시지가 없어도 멈추지 않아 게임 루프에 적합
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT) s_should_close = true;
        TranslateMessage(&msg);   // WM_KEYDOWN → WM_CHAR 변환 (문자 입력용)
        DispatchMessageA(&msg);   // WndProc 호출
    }

    // 델타타임: QueryPerformanceCounter 두 번의 차이 / 주파수
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - s_frame_start.QuadPart)
             / (float)s_freq.QuadPart;
    s_frame_start = now;

    // 창 드래그 등으로 발생하는 스파이크 클램핑 (원본 main.cpp 와 동일)
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

void platform_end_frame()
{
    // 더블 버퍼 교체: 백 버퍼(GPU가 그린 것) → 프론트 버퍼(화면)
    // raylib::EndDrawing()이 내부에서 호출하는 것과 동일
    SwapBuffers(s_hdc);
}

bool platform_key_pressed(int key)
{
    if (key < 0 || key >= 256) return false;
    // 이번 프레임에 눌렸고 && 이전 프레임엔 안 눌렸으면 → "방금 눌림"
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
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - s_init_time.QuadPart)
         / (double)s_freq.QuadPart;
}

void* platform_get_hdc()
{
    return (void*)s_hdc;
}

int platform_win_height()
{
    return s_win_h;
}
