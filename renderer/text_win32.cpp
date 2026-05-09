// renderer/text_win32.cpp — GDI + wglUseFontBitmaps 텍스트 백엔드
//
// 이 파일은 renderer.cpp 에서 분리된 Win32 전용 텍스트 경로다.
// 크로스플랫폼 빌드(SDL2)에서는 renderer/text_stb.cpp 가 대신 사용된다.
//
// 구현: GDI 로 TTF 를 불러오고, wglUseFontBitmapsA 로 문자당 OpenGL
//       디스플레이 리스트를 만든 뒤 glCallLists 로 렌더링한다.
//       glCallLists/glWindowPos2i 는 OpenGL 1.x compatibility profile 에서만 동작.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cstring>
#include <cstdio>
#include <map>
#include "renderer.h"
#include "../platform/platform.h"
#include "../platform/gl_defs.h"

extern void (APIENTRY *glUseProgram)(GLuint);
extern void (APIENTRY *glWindowPos2i)(GLint, GLint);

static int    s_text_screen_h = 0;
static char   s_font_face[128] = "monogram";
static std::map<int, GLuint> s_font_lists;
static std::map<int, HFONT>  s_font_cache;

void renderer_text_set_screen_height(int h) { s_text_screen_h = h; }

void renderer_load_font(const char* path)
{
    int added = AddFontResourceExA(path, FR_PRIVATE, nullptr);
    if (added == 0) {
        fprintf(stderr, "[FONT] AddFontResourceEx failed: %s\n", path);
        strncpy(s_font_face, "Courier New", sizeof(s_font_face) - 1);
    } else {
        strncpy(s_font_face, "monogram", sizeof(s_font_face) - 1);
    }
}

static GLuint get_font_list(int size)
{
    auto it = s_font_lists.find(size);
    if (it != s_font_lists.end()) return it->second;

    HFONT hfont = CreateFontA(
        size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, s_font_face);
    if (!hfont) return 0;

    HDC   hdc      = (HDC)platform_get_hdc();
    HFONT old_font = (HFONT)SelectObject(hdc, hfont);

    GLuint base = glGenLists(96);
    wglUseFontBitmapsA(hdc, 32, 96, base);

    SelectObject(hdc, old_font);
    DeleteObject(hfont);
    s_font_lists[size] = base;
    return base;
}

static HFONT get_cached_font(int size)
{
    auto it = s_font_cache.find(size);
    if (it != s_font_cache.end()) return it->second;
    HFONT hfont = CreateFontA(
        size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, s_font_face);
    if (hfont) s_font_cache[size] = hfont;
    return hfont;
}

void draw_text(const char* text, int x, int y, int size, Color c)
{
    if (!text || !text[0]) return;
    if (!glWindowPos2i) return;
    GLuint base = get_font_list(size);
    if (!base) return;

    glUseProgram(0);
    glColor4ub(c.r, c.g, c.b, c.a);
    glWindowPos2i(x, s_text_screen_h - y - size);
    glListBase(base - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
}

int measure_text(const char* text, int size)
{
    if (!text || !text[0]) return 0;
    HFONT hfont = get_cached_font(size);
    if (!hfont) return (int)(strlen(text) * size * 0.6f);

    HDC   hdc = (HDC)platform_get_hdc();
    HFONT old = (HFONT)SelectObject(hdc, hfont);
    SIZE  sz;
    GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
    SelectObject(hdc, old);
    return sz.cx;
}

void renderer_text_shutdown()
{
    for (auto& [sz, base] : s_font_lists) glDeleteLists(base, 96);
    s_font_lists.clear();
    for (auto& [sz, h] : s_font_cache) DeleteObject(h);
    s_font_cache.clear();
}
