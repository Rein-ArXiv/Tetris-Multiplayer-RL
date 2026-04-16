// renderer/renderer.cpp — OpenGL 2D 렌더러 (rect / rounded rect)
//
// 텍스트 렌더링은 백엔드별로 분리:
//   - renderer/text_win32.cpp : GDI + wglUseFontBitmaps (Windows 기본)
//   - renderer/text_stb.cpp   : stb_easy_font (SDL2 빌드에서 사용)
//
// 학습 포인트:
//   이 파일이 하는 일 = raylib 의 rlgl.h + rshapes.c 에 해당.
//   핵심 개념: VAO(레이아웃) + VBO(GPU 버퍼) + 셰이더 프로그램.

#include <cstring>
#include <cstdio>
#include <cmath>
#include "renderer.h"
#include "shaders.h"
#include "../platform/gl_defs.h"

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <GL/gl.h>
#elif defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl.h>
#else
  #include <GL/gl.h>
#endif

// ─── GL 함수 포인터 (platform/*.cpp 에서 정의, 여기서 extern 선언) ───────────
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
extern void   (APIENTRY *glDeleteVertexArrays)(GLsizei, const GLuint*);
extern void   (APIENTRY *glDeleteBuffers)(GLsizei, const GLuint*);
extern void   (APIENTRY *glDeleteProgram)(GLuint);

// 텍스트 백엔드 — 화면 높이 전달용 훅 (win32/stb 양쪽 모두 정의)
void renderer_text_set_screen_height(int h);
void renderer_text_shutdown();

// ─── 렌더러 상태 ──────────────────────────────────────────────────────────────
static int    s_screen_w = 0;
static int    s_screen_h = 0;

static GLuint s_rect_prog  = 0;
static GLuint s_rect_vao   = 0;
static GLuint s_rect_vbo   = 0;
static GLint  s_rect_proj  = -1;
static GLint  s_rect_color = -1;

static float  s_proj[16];
static int    s_view_ox = 0;
static int    s_view_oy = 0;

// text 백엔드(stb) 가 공용으로 사용할 수 있게 셰이더/VAO 접근자 노출
GLuint renderer_get_rect_prog()  { return s_rect_prog;  }
GLuint renderer_get_rect_vao()   { return s_rect_vao;   }
GLuint renderer_get_rect_vbo()   { return s_rect_vbo;   }
GLint  renderer_get_rect_proj()  { return s_rect_proj;  }
GLint  renderer_get_rect_color() { return s_rect_color; }
const float* renderer_get_proj() { return s_proj;       }
int    renderer_get_screen_h()   { return s_screen_h;   }

static GLuint compile_shader(GLenum type, const char* src)
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
        fprintf(stderr, "[GLSL] Compile error:\n%s\n", log);
    }
    return s;
}

static GLuint link_program(const char* vert_src, const char* frag_src)
{
    GLuint v = compile_shader(GL_VERTEX_SHADER,   vert_src);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, frag_src);
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
        fprintf(stderr, "[GLSL] Link error:\n%s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

static void build_ortho(float* m, float w, float h)
{
    float l = 0.0f, r = w, t = 0.0f, b = h, n = -1.0f, f = 1.0f;
    memset(m, 0, 16 * sizeof(float));
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
}

void renderer_init(int screen_w, int screen_h)
{
    s_screen_w = screen_w;
    s_screen_h = screen_h;
    build_ortho(s_proj, (float)screen_w, (float)screen_h);
    renderer_text_set_screen_height(screen_h);

    s_rect_prog  = link_program(kRectVert, kRectFrag);
    s_rect_proj  = glGetUniformLocation(s_rect_prog, "u_proj");
    s_rect_color = glGetUniformLocation(s_rect_prog, "u_color");

    glGenVertexArrays(1, &s_rect_vao);
    glBindVertexArray(s_rect_vao);

    glGenBuffers(1, &s_rect_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void renderer_begin(Color bg)
{
    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, bg.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_end() {}

void renderer_set_view_offset(int dx, int dy)
{
    s_view_ox = dx;
    s_view_oy = dy;
    // 시야를 (dx, dy) 만큼 옮기려면 ortho 의 l/r/t/b 를 정반대로 시프트.
    // 예: dx=+5 → 모든 것을 오른쪽으로 5px 이동시키려면 투영창의 원점을 왼쪽으로 5px.
    float l = (float)(-dx),          r = (float)(s_screen_w - dx);
    float t = (float)(-dy),          b = (float)(s_screen_h - dy);
    float n = -1.0f, f = 1.0f;
    memset(s_proj, 0, 16 * sizeof(float));
    s_proj[0]  =  2.0f / (r - l);
    s_proj[5]  =  2.0f / (t - b);
    s_proj[10] = -2.0f / (f - n);
    s_proj[12] = -(r + l) / (r - l);
    s_proj[13] = -(t + b) / (t - b);
    s_proj[14] = -(f + n) / (f - n);
    s_proj[15] = 1.0f;
}

void renderer_shutdown()
{
    renderer_text_shutdown();
    if (s_rect_prog) { glDeleteProgram(s_rect_prog); s_rect_prog = 0; }
    if (s_rect_vbo)  { glDeleteBuffers(1, &s_rect_vbo); s_rect_vbo = 0; }
    if (s_rect_vao)  { glDeleteVertexArrays(1, &s_rect_vao); s_rect_vao = 0; }
}

void draw_rect(int x, int y, int w, int h, Color c)
{
    float fx = (float)x, fy = (float)y;
    float fw = (float)w, fh = (float)h;
    float verts[12] = {
        fx,      fy,
        fx,      fy + fh,
        fx + fw, fy + fh,
        fx,      fy,
        fx + fw, fy + fh,
        fx + fw, fy,
    };
    glUseProgram(s_rect_prog);
    glUniformMatrix4fv(s_rect_proj, 1, GL_FALSE, s_proj);
    glUniform4f(s_rect_color,
        c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);

    glBindVertexArray(s_rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c)
{
    float minDim = (float)(w < h ? w : h);
    float r = roundness * minDim * 0.5f;
    if (r < 1.0f) { draw_rect(x, y, w, h, c); return; }

    float fx = (float)x, fy = (float)y, fw = (float)w, fh = (float)h;

    const int N_SEG = 8;
    const int MAX_VERTS = 18 + 4 * N_SEG * 3;
    float verts[MAX_VERTS * 2];
    int vi = 0;

    auto V = [&](float px, float py) { verts[vi++] = px; verts[vi++] = py; };
    auto Rect = [&](float rx, float ry, float rw, float rh) {
        V(rx, ry);       V(rx, ry+rh);       V(rx+rw, ry+rh);
        V(rx, ry);       V(rx+rw, ry+rh);    V(rx+rw, ry);
    };

    Rect(fx + r, fy,          fw - 2*r, fh);
    Rect(fx,     fy + r,      r,        fh - 2*r);
    Rect(fx + fw - r, fy + r, r,        fh - 2*r);

    const float PI = 3.14159265358979f;
    auto Corner = [&](float cx, float cy, float startAngle) {
        float step = (PI * 0.5f) / N_SEG;
        for (int i = 0; i < N_SEG; ++i) {
            float a0 = startAngle + step * i;
            float a1 = startAngle + step * (i + 1);
            V(cx, cy);
            V(cx + r * cosf(a0), cy + r * sinf(a0));
            V(cx + r * cosf(a1), cy + r * sinf(a1));
        }
    };

    Corner(fx + r,      fy + r,      PI);
    Corner(fx + fw - r, fy + r,      PI * 1.5f);
    Corner(fx + fw - r, fy + fh - r, 0.0f);
    Corner(fx + r,      fy + fh - r, PI * 0.5f);

    int numVerts = vi / 2;

    glUseProgram(s_rect_prog);
    glUniformMatrix4fv(s_rect_proj, 1, GL_FALSE, s_proj);
    glUniform4f(s_rect_color,
        c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);

    glBindVertexArray(s_rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, vi * sizeof(float), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, numVerts);
    glBindVertexArray(0);
}
