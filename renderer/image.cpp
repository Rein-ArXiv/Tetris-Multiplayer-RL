// renderer/image.cpp — PNG/JPG 로더 + 텍스처 쿼드 렌더
//
// Win32 경로에서는 GDI+ (gdiplus.lib) 가 PNG/JPG/BMP 디코딩을 담당한다.
// 다른 플랫폼(Linux/macOS)에서는 third_party/stb_image.h 가 PNG/JPG 등을 디코딩한다.

#include "image.h"
#include "shaders.h"
#include "../platform/gl_defs.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
  // NOMINMAX 금지 — <gdiplus.h> 가 전역 min/max 매크로를 요구.
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <objidl.h>        // IStream (gdiplus prerequisites)
  #include <algorithm>       // std::min/std::max — gdiplus namespace fallback
  using std::min; using std::max;
  #include <gdiplus.h>
  #include <GL/gl.h>
  #pragma comment(lib, "gdiplus.lib")
#else
  #include <GL/gl.h>
  #define STB_IMAGE_IMPLEMENTATION
  #include "../third_party/stb_image.h"
#endif

// ─── 외부 GL 심볼 (win32.cpp 에서 wglGetProcAddress 로 채움) ─────────────────
extern GLuint (APIENTRY *glCreateShader)(GLenum);
extern void   (APIENTRY *glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void   (APIENTRY *glCompileShader)(GLuint);
extern GLuint (APIENTRY *glCreateProgram)(void);
extern void   (APIENTRY *glAttachShader)(GLuint, GLuint);
extern void   (APIENTRY *glLinkProgram)(GLuint);
extern void   (APIENTRY *glUseProgram)(GLuint);
extern void   (APIENTRY *glDeleteShader)(GLuint);
extern GLint  (APIENTRY *glGetUniformLocation)(GLuint, const GLchar*);
extern void   (APIENTRY *glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void   (APIENTRY *glUniform1i)(GLint, GLint);
extern void   (APIENTRY *glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void   (APIENTRY *glGenVertexArrays)(GLsizei, GLuint*);
extern void   (APIENTRY *glBindVertexArray)(GLuint);
extern void   (APIENTRY *glGenBuffers)(GLsizei, GLuint*);
extern void   (APIENTRY *glBindBuffer)(GLenum, GLuint);
extern void   (APIENTRY *glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void   (APIENTRY *glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void   (APIENTRY *glEnableVertexAttribArray)(GLuint);
extern void   (APIENTRY *glActiveTextureProc)(GLenum);
extern void   (APIENTRY *glDeleteVertexArrays)(GLsizei, const GLuint*);
extern void   (APIENTRY *glDeleteBuffers)(GLsizei, const GLuint*);
extern void   (APIENTRY *glDeleteProgram)(GLuint);
extern void   (APIENTRY *glGetShaderiv)(GLuint, GLenum, GLint*);
extern void   (APIENTRY *glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void   (APIENTRY *glGetProgramiv)(GLuint, GLenum, GLint*);
extern void   (APIENTRY *glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);

// renderer.cpp 에서 제공하는 프로젝션 매트릭스 + 화면 정보
extern const float* renderer_get_proj();

#ifndef GL_BLEND
#define GL_BLEND 0x0BE2
#endif
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_LINEAR
#define GL_LINEAR 0x2601
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER 0x2801
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER 0x2800
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S 0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#define GL_TEXTURE_WRAP_T 0x2803
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif

// ─── 상태 ────────────────────────────────────────────────────────────────────
struct ImageEntry {
    bool    used = false;
    GLuint  tex  = 0;
    int     w    = 0;
    int     h    = 0;
};
static std::vector<ImageEntry> s_images;  // index 0 은 예약 (invalid handle)

static GLuint s_sprite_prog = 0;
static GLuint s_sprite_vao  = 0;
static GLuint s_sprite_vbo  = 0;
static GLint  s_sprite_proj = -1;
static GLint  s_sprite_tint = -1;
static GLint  s_sprite_tex  = -1;

#if defined(_WIN32)
static ULONG_PTR s_gdiplus_token = 0;
static bool      s_gdiplus_initialized = false;
#endif

// ─── GDI+ 디코더 (Win32) ────────────────────────────────────────────────────
//  성공 시 outPixels 에 RGBA8 row-major, width/height 채움.
static bool decode_image_win32(const char* path, std::vector<uint8_t>& outPixels,
                               int& outW, int& outH)
{
#if defined(_WIN32)
    if (!s_gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput si;
        if (Gdiplus::GdiplusStartup(&s_gdiplus_token, &si, nullptr) != Gdiplus::Ok) {
            fprintf(stderr, "[IMG] GdiplusStartup failed\n");
            return false;
        }
        s_gdiplus_initialized = true;
    }

    // UTF-8 → UTF-16
    int wn = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wn <= 0) return false;
    std::wstring wpath(wn, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wn);
    wpath.resize(wn - 1);

    Gdiplus::Bitmap bmp(wpath.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        fprintf(stderr, "[IMG] load failed: %s\n", path);
        return false;
    }

    outW = (int)bmp.GetWidth();
    outH = (int)bmp.GetHeight();
    if (outW <= 0 || outH <= 0) return false;

    Gdiplus::BitmapData bd{};
    Gdiplus::Rect rc(0, 0, outW, outH);
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead,
                     PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        fprintf(stderr, "[IMG] LockBits failed: %s\n", path);
        return false;
    }

    // GDI+ 포맷은 BGRA (premultiplied 아님). OpenGL 은 RGBA 기대.
    // row stride 는 bd.Stride (양수, top-down bitmap 의 경우).
    outPixels.assign((size_t)outW * outH * 4, 0);
    const uint8_t* srcBase = (const uint8_t*)bd.Scan0;
    for (int y = 0; y < outH; ++y) {
        const uint8_t* srcRow = srcBase + (ptrdiff_t)y * bd.Stride;
        uint8_t* dstRow = outPixels.data() + (size_t)y * outW * 4;
        for (int x = 0; x < outW; ++x) {
            uint8_t b = srcRow[x * 4 + 0];
            uint8_t g = srcRow[x * 4 + 1];
            uint8_t r = srcRow[x * 4 + 2];
            uint8_t a = srcRow[x * 4 + 3];
            dstRow[x * 4 + 0] = r;
            dstRow[x * 4 + 1] = g;
            dstRow[x * 4 + 2] = b;
            dstRow[x * 4 + 3] = a;
        }
    }
    bmp.UnlockBits(&bd);
    return true;
#else
    // 비-Win32: stb_image 로 디코드 (강제 RGBA8 row-major).
    int n = 0;
    unsigned char* data = stbi_load(path, &outW, &outH, &n, 4);
    if (!data) {
        fprintf(stderr, "[IMG] stbi_load failed: %s (%s)\n", path, stbi_failure_reason());
        return false;
    }
    if (outW <= 0 || outH <= 0) { stbi_image_free(data); return false; }
    outPixels.assign(data, data + (size_t)outW * outH * 4);
    stbi_image_free(data);
    return true;
#endif
}

// ─── 셰이더 초기화 — renderer.cpp 패턴과 동일 ───────────────────────────────
static GLuint compile_shader_s(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; GLsizei len = 0;
        glGetShaderInfoLog(s, sizeof(log), &len, log);
        log[len < (GLsizei)sizeof(log) ? len : sizeof(log) - 1] = '\0';
        fprintf(stderr, "[SPRITE GLSL] Compile error:\n%s\n", log);
    }
    return s;
}

static GLuint link_program_s(const char* vs, const char* fs)
{
    GLuint v = compile_shader_s(GL_VERTEX_SHADER,   vs);
    GLuint f = compile_shader_s(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; GLsizei len = 0;
        glGetProgramInfoLog(p, sizeof(log), &len, log);
        log[len < (GLsizei)sizeof(log) ? len : sizeof(log) - 1] = '\0';
        fprintf(stderr, "[SPRITE GLSL] Link error:\n%s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

void image_init()
{
    if (s_sprite_prog) return;  // 중복 호출 방어

    s_sprite_prog = link_program_s(kSpriteVert, kSpriteFrag);
    s_sprite_proj = glGetUniformLocation(s_sprite_prog, "u_proj");
    s_sprite_tint = glGetUniformLocation(s_sprite_prog, "u_tint");
    s_sprite_tex  = glGetUniformLocation(s_sprite_prog, "u_tex");

    glGenVertexArrays(1, &s_sprite_vao);
    glBindVertexArray(s_sprite_vao);

    glGenBuffers(1, &s_sprite_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_sprite_vbo);
    // 6 vertex × (xy+uv) = 24 float
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // a_pos = location 0, a_uv = location 1
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (const void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (const void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // 핸들 0 예약
    if (s_images.empty()) s_images.push_back({});
}

void image_shutdown()
{
    for (auto& e : s_images) {
        if (e.used && e.tex) glDeleteTextures(1, &e.tex);
        e = {};
    }
    s_images.clear();

    if (s_sprite_vbo && glDeleteBuffers)       glDeleteBuffers(1, &s_sprite_vbo);
    if (s_sprite_vao && glDeleteVertexArrays)  glDeleteVertexArrays(1, &s_sprite_vao);
    if (s_sprite_prog && glDeleteProgram)      glDeleteProgram(s_sprite_prog);
    s_sprite_vbo = s_sprite_vao = s_sprite_prog = 0;
    s_sprite_proj = s_sprite_tint = s_sprite_tex = -1;

#if defined(_WIN32)
    if (s_gdiplus_initialized) {
        Gdiplus::GdiplusShutdown(s_gdiplus_token);
        s_gdiplus_initialized = false;
    }
#endif
}

// ─── 로드/해제 ───────────────────────────────────────────────────────────────
static ImageHandle image_create_texture_from_rgba(const uint8_t* pixels, int w, int h)
{
    if (!pixels || w <= 0 || h <= 0) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 빈 슬롯 재사용 또는 push
    for (size_t i = 1; i < s_images.size(); ++i) {
        if (!s_images[i].used) {
            s_images[i] = {true, tex, w, h};
            return (ImageHandle)i;
        }
    }
    s_images.push_back({true, tex, w, h});
    return (ImageHandle)(s_images.size() - 1);
}

ImageHandle image_load(const char* path)
{
    if (!path || !*path) return 0;

    std::vector<uint8_t> pixels;
    int w = 0, h = 0;
    if (!decode_image_win32(path, pixels, w, h)) return 0;
    return image_create_texture_from_rgba(pixels.data(), w, h);
}

ImageHandle image_create_rgba(const uint8_t* pixels, int w, int h)
{
    return image_create_texture_from_rgba(pixels, w, h);
}

void image_unload(ImageHandle h)
{
    if (h <= 0 || (size_t)h >= s_images.size()) return;
    auto& e = s_images[h];
    if (!e.used) return;
    if (e.tex) glDeleteTextures(1, &e.tex);
    e = {};
}

bool image_size(ImageHandle h, int& w_out, int& h_out)
{
    if (h <= 0 || (size_t)h >= s_images.size() || !s_images[h].used) return false;
    w_out = s_images[h].w;
    h_out = s_images[h].h;
    return true;
}

// ─── 드로우 ──────────────────────────────────────────────────────────────────
static void draw_image_impl(ImageHandle h, int x, int y, int w, int ht,
                            float tr, float tg, float tb, float ta)
{
    if (h <= 0 || (size_t)h >= s_images.size() || !s_images[h].used) return;
    if (!s_sprite_prog) return;

    float fx = (float)x, fy = (float)y;
    float fw = (float)w, fh = (float)ht;

    // (xy, uv) 6 vertex. UV 의 v 축은 텍스처 상단 = 0, 하단 = 1.
    // GDI+ 비트맵은 top-down 으로 읽었으므로 v=0 이 top. OpenGL 텍스처의
    // y 는 기본적으로 bottom 이지만 glTexImage2D 에서 첫 row 가 v=0 으로 저장됨.
    float verts[24] = {
        // pos        uv
        fx,      fy,         0.0f, 0.0f,
        fx,      fy + fh,    0.0f, 1.0f,
        fx + fw, fy + fh,    1.0f, 1.0f,
        fx,      fy,         0.0f, 0.0f,
        fx + fw, fy + fh,    1.0f, 1.0f,
        fx + fw, fy,         1.0f, 0.0f,
    };

    glUseProgram(s_sprite_prog);
    glUniformMatrix4fv(s_sprite_proj, 1, GL_FALSE, renderer_get_proj());
    glUniform4f(s_sprite_tint, tr, tg, tb, ta);
    glUniform1i(s_sprite_tex, 0);  // sampler → TEXTURE0

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (glActiveTextureProc) glActiveTextureProc(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_images[h].tex);

    glBindVertexArray(s_sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_sprite_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    // 주의: GL_BLEND 는 켠 채로 둔다. draw_rect 계열은 알파 255 면 결과 동일.
}

void draw_image(ImageHandle h, int x, int y, int w, int h_px)
{
    draw_image_impl(h, x, y, w, h_px, 1.0f, 1.0f, 1.0f, 1.0f);
}

void draw_image_tinted(ImageHandle h, int x, int y, int w, int h_px, Color tint)
{
    draw_image_impl(h, x, y, w, h_px,
                    tint.r / 255.0f, tint.g / 255.0f,
                    tint.b / 255.0f, tint.a / 255.0f);
}
