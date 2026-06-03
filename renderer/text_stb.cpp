// renderer/text_stb.cpp — 크로스플랫폼 텍스트 백엔드 (stb_truetype)
//
// SDL2 빌드(Mac/Linux)에서 사용. 순수 OpenGL 코어 프로파일이라
// wglUseFontBitmaps / glCallLists 같은 compat-profile API를 쓸 수 없다.
//
// 구현 전략:
//   - renderer_load_font 로 받은 TTF 를 stb_truetype 로 파싱.
//   - draw_text 는 UTF-8 을 코드포인트로 디코드한 뒤, 코드포인트별 글리프를
//     온디맨드로 래스터화해 단일 GL_R8 텍스처 아틀라스(shelf packing)에 캐시.
//   - 캐시된 글리프들을 textured quad 로 모아 한 번의 draw call 로 렌더.
//
// 한글 지원: 5x7 비트맵(ASCII 전용) 대신 실제 TTF 를 쓰므로, 폰트에 글리프가
//   있는 한 임의의 유니코드(한글 포함)를 렌더할 수 있다.
//
// 셰이더: 위치(xy)와 UV(zw)를 하나의 vec4 속성으로 묶어 attribute location 을
//   0 하나만 쓴다(rect 셰이더와 동일 가정). glGetAttribLocation 불필요.

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include "renderer.h"
#include "../platform/gl_defs.h"

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl.h>
#elif defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <GL/gl.h>
#else
  #include <GL/gl.h>
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

// ─── GL 함수 포인터 (platform/*.cpp 에서 정의, 여기서 extern) ────────────────
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
extern void   (APIENTRY *glGetShaderiv)(GLuint, GLenum, GLint*);
extern void   (APIENTRY *glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void   (APIENTRY *glGetProgramiv)(GLuint, GLenum, GLint*);
extern void   (APIENTRY *glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);

extern const float* renderer_get_proj();

#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_LINEAR 0x2601
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif

// ─── 셰이더 (위치 xy + UV zw 를 단일 vec4 속성으로) ──────────────────────────
static const char* kFontVert = R"glsl(
#version 130
in vec4 a_posuv;
out vec2 v_uv;
uniform mat4 u_proj;
void main() {
    v_uv = a_posuv.zw;
    gl_Position = u_proj * vec4(a_posuv.xy, 0.0, 1.0);
}
)glsl";

static const char* kFontFrag = R"glsl(
#version 130
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D u_tex;
uniform vec4 u_color;
void main() {
    float a = texture(u_tex, v_uv).r;
    fragColor = vec4(u_color.rgb, u_color.a * a);
}
)glsl";

// ─── 상태 ─────────────────────────────────────────────────────────────────────
static constexpr int ATLAS_W = 1024;
static constexpr int ATLAS_H = 1024;
static constexpr int PAD     = 1;   // 글리프 간 패딩(샘플링 번짐 방지)

struct Glyph {
    float u0, v0, u1, v1;   // 아틀라스 UV
    int   w, h;             // 비트맵 크기(px)
    int   xoff, yoff;       // 베이스라인 기준 배치 오프셋
    float advance;          // 다음 글리프까지 진행(px)
    bool  has_pixels;       // 공백 등은 false
};

static stbtt_fontinfo      s_font;
static std::vector<uint8_t> s_ttf;          // TTF 원본 버퍼(폰트 수명 동안 유지 필요)
static bool                s_font_ok = false;

static GLuint s_prog = 0, s_vao = 0, s_vbo = 0, s_atlas = 0;
static GLint  s_u_proj = -1, s_u_color = -1, s_u_tex = -1;
static bool   s_gl_ready = false;

// shelf packer 커서
static int s_cur_x = PAD, s_cur_y = PAD, s_shelf_h = 0;

// (codepoint, pixelHeight) → Glyph
static std::unordered_map<uint64_t, Glyph> s_cache;

static int s_screen_h = 0;
void renderer_text_set_screen_height(int h) { s_screen_h = h; }

// ─── 셰이더 컴파일 ────────────────────────────────────────────────────────────
static GLuint compile(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; GLsizei n = 0; glGetShaderInfoLog(s, sizeof(log), &n, log);
        log[n < (GLsizei)sizeof(log) ? n : (GLsizei)sizeof(log) - 1] = '\0';
        fprintf(stderr, "[text] shader compile error:\n%s\n", log);
    }
    return s;
}

static void ensure_gl()
{
    if (s_gl_ready) return;
    s_gl_ready = true;

    GLuint v = compile(GL_VERTEX_SHADER,   kFontVert);
    GLuint f = compile(GL_FRAGMENT_SHADER, kFontFrag);
    s_prog = glCreateProgram();
    glAttachShader(s_prog, v);
    glAttachShader(s_prog, f);
    glLinkProgram(s_prog);
    GLint ok = 0; glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; GLsizei n = 0; glGetProgramInfoLog(s_prog, sizeof(log), &n, log);
        log[n < (GLsizei)sizeof(log) ? n : (GLsizei)sizeof(log) - 1] = '\0';
        fprintf(stderr, "[text] program link error:\n%s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);

    s_u_proj  = glGetUniformLocation(s_prog, "u_proj");
    s_u_color = glGetUniformLocation(s_prog, "u_color");
    s_u_tex   = glGetUniformLocation(s_prog, "u_tex");

    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    // a_posuv: vec4 (x, y, u, v) — 단일 속성, location 0
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // 빈 GL_R8 아틀라스 텍스처 생성
    glGenTextures(1, &s_atlas);
    glBindTexture(GL_TEXTURE_2D, s_atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // 1바이트 정렬(R8 행)
    std::vector<uint8_t> zero((size_t)ATLAS_W * ATLAS_H, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, zero.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void renderer_load_font(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "[text] font open failed: %s\n", path); return; }
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); fprintf(stderr, "[text] font empty: %s\n", path); return; }
    s_ttf.resize((size_t)sz);
    size_t rd = fread(s_ttf.data(), 1, (size_t)sz, fp);
    fclose(fp);
    if (rd != (size_t)sz) { fprintf(stderr, "[text] font read short: %s\n", path); return; }

    if (!stbtt_InitFont(&s_font, s_ttf.data(),
                        stbtt_GetFontOffsetForIndex(s_ttf.data(), 0))) {
        fprintf(stderr, "[text] stbtt_InitFont failed: %s\n", path);
        return;
    }
    s_font_ok = true;
    s_cache.clear();
    s_cur_x = PAD; s_cur_y = PAD; s_shelf_h = 0;
}

// ─── UTF-8 → 코드포인트 (한 글자 디코드, 포인터 전진) ────────────────────────
static uint32_t utf8_next(const char** p)
{
    const uint8_t* s = (const uint8_t*)*p;
    uint32_t cp; int n;
    if (s[0] < 0x80)            { cp = s[0];          n = 1; }
    else if ((s[0] & 0xE0)==0xC0){ cp = s[0] & 0x1F;  n = 2; }
    else if ((s[0] & 0xF0)==0xE0){ cp = s[0] & 0x0F;  n = 3; }
    else if ((s[0] & 0xF8)==0xF0){ cp = s[0] & 0x07;  n = 4; }
    else                         { *p += 1; return 0xFFFD; } // 잘못된 선두 바이트
    for (int i = 1; i < n; ++i) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return 0xFFFD; } // 끊긴 시퀀스
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += n;
    return cp;
}

// ─── 글리프 가져오기(없으면 래스터화 후 아틀라스에 적재) ─────────────────────
static const Glyph* get_glyph(uint32_t cp, int px)
{
    if (px < 1) px = 1;
    uint64_t key = ((uint64_t)cp << 16) | (uint32_t)px;
    auto it = s_cache.find(key);
    if (it != s_cache.end()) return &it->second;

    float scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&s_font, (int)cp, &adv, &lsb);

    int gw, gh, xoff, yoff;
    unsigned char* bmp = stbtt_GetCodepointBitmap(
        &s_font, scale, scale, (int)cp, &gw, &gh, &xoff, &yoff);

    Glyph g{};
    g.advance = adv * scale;
    g.xoff = xoff; g.yoff = yoff;
    g.w = gw; g.h = gh;
    g.has_pixels = (bmp && gw > 0 && gh > 0);

    if (g.has_pixels) {
        // shelf packing: 현재 행에 안 들어가면 다음 행으로
        if (s_cur_x + gw + PAD > ATLAS_W) {
            s_cur_x = PAD;
            s_cur_y += s_shelf_h + PAD;
            s_shelf_h = 0;
        }
        if (s_cur_y + gh + PAD > ATLAS_H) {
            // 아틀라스 가득 — 이 글리프는 비워두고 advance 만 유지
            fprintf(stderr, "[text] glyph atlas full (cp=U+%04X)\n", cp);
            g.has_pixels = false;
        } else {
            int gx = s_cur_x, gy = s_cur_y;
            glBindTexture(GL_TEXTURE_2D, s_atlas);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, gx, gy, gw, gh,
                            GL_RED, GL_UNSIGNED_BYTE, bmp);
            g.u0 = (float)gx / ATLAS_W;       g.v0 = (float)gy / ATLAS_H;
            g.u1 = (float)(gx + gw) / ATLAS_W; g.v1 = (float)(gy + gh) / ATLAS_H;
            s_cur_x += gw + PAD;
            if (gh > s_shelf_h) s_shelf_h = gh;
        }
    }
    if (bmp) stbtt_FreeBitmap(bmp, nullptr);

    auto res = s_cache.emplace(key, g);
    return &res.first->second;
}

// ─── 측정 ─────────────────────────────────────────────────────────────────────
int measure_text(const char* text, int size)
{
    if (!text || !text[0] || !s_font_ok) return 0;
    int px = size < 1 ? 1 : size;
    float w = 0.0f;
    for (const char* p = text; *p; ) {
        uint32_t cp = utf8_next(&p);
        const Glyph* g = get_glyph(cp, px);
        w += g->advance;
    }
    return (int)(w + 0.5f);
}

// ─── 렌더 ─────────────────────────────────────────────────────────────────────
void draw_text(const char* text, int x, int y, int size, Color c)
{
    if (!text || !text[0] || !s_font_ok) return;
    ensure_gl();
    int px = size < 1 ? 1 : size;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&s_font, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
    float baseline = (float)y + ascent * scale;   // (x,y)=텍스트 좌상단 기준

    std::vector<float> verts;
    verts.reserve(strlen(text) * 6 * 4);

    auto emit = [&](float qx, float qy, float qw, float qh,
                    float u0, float v0, float u1, float v1) {
        verts.insert(verts.end(), {
            qx,      qy,      u0, v0,
            qx,      qy + qh, u0, v1,
            qx + qw, qy + qh, u1, v1,
            qx,      qy,      u0, v0,
            qx + qw, qy + qh, u1, v1,
            qx + qw, qy,      u1, v0,
        });
    };

    float penX = (float)x;
    for (const char* p = text; *p; ) {
        uint32_t cp = utf8_next(&p);
        const Glyph* g = get_glyph(cp, px);
        if (g->has_pixels) {
            float qx = penX + g->xoff;
            float qy = baseline + g->yoff;
            emit(qx, qy, (float)g->w, (float)g->h, g->u0, g->v0, g->u1, g->v1);
        }
        penX += g->advance;
    }

    if (verts.empty()) return;

    glUseProgram(s_prog);
    glUniformMatrix4fv(s_u_proj, 1, GL_FALSE, renderer_get_proj());
    glUniform4f(s_u_color, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
    glUniform1i(s_u_tex, 0);

    glBindTexture(GL_TEXTURE_2D, s_atlas);   // 유닛 0(기본)에 바인딩
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
    glBindVertexArray(0);
}

void renderer_text_shutdown()
{
    s_cache.clear();
    s_ttf.clear();
    s_font_ok = false;
    s_gl_ready = false;
}
