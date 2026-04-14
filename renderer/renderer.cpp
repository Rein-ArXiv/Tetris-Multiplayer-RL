// renderer/renderer.cpp — OpenGL 2D 렌더러 구현
//
// 학습 포인트:
//
//   이 파일이 하는 일 = raylib의 rlgl.h + rshapes.c + rtext.c 에 해당합니다.
//
//   핵심 개념:
//   ┌─ VAO (Vertex Array Object)
//   │   "이 VBO를 이 레이아웃으로 읽어라"는 설정 묶음. 한 번 설정하면 재사용.
//   │
//   ├─ VBO (Vertex Buffer Object)
//   │   GPU 메모리에 있는 꼭짓점 데이터 배열.
//   │   glBufferData: CPU → GPU 메모리 전송.
//   │
//   ├─ 셰이더 프로그램
//   │   glCreateShader → glShaderSource → glCompileShader (각 셰이더 컴파일)
//   │   glCreateProgram → glAttachShader → glLinkProgram  (링크)
//   │   glUseProgram: 이후 draw call에 이 셰이더를 사용
//   │
//   └─ draw_rect(x,y,w,h,c) 한 번 호출의 실제 흐름:
//      1. 6개 꼭짓점 계산 (2개 삼각형 = 1개 사각형)
//      2. glBufferData → VBO에 꼭짓점 업로드 (CPU → GPU)
//      3. glUniform4f → 색상 uniform 업로드
//      4. glDrawArrays(GL_TRIANGLES, 0, 6) → GPU가 삼각형 그림

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <map>
#include "renderer.h"
#include "shaders.h"

// ─── GL 함수 포인터 (win32.cpp 에서 정의, 여기서 extern 선언) ────────────────
#include "../platform/gl_defs.h"  // GLchar, GLsizeiptr, GL 상수 (win32.cpp 와 공유)

// win32.cpp에서 정의된 GL 함수 포인터들을 extern으로 선언
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
extern void   (APIENTRY *glUniform1i)(GLint, GLint);
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
extern void   (APIENTRY *glWindowPos2i)(GLint, GLint);
extern void   (APIENTRY *glDeleteVertexArrays)(GLsizei, const GLuint*);
extern void   (APIENTRY *glDeleteBuffers)(GLsizei, const GLuint*);
extern void   (APIENTRY *glDeleteProgram)(GLuint);

// ─── 렌더러 상태 ──────────────────────────────────────────────────────────────
static int    s_screen_w = 0;
static int    s_screen_h = 0;

// Rect 셰이더
static GLuint s_rect_prog  = 0;
static GLuint s_rect_vao   = 0;
static GLuint s_rect_vbo   = 0;
static GLint  s_rect_proj  = -1;
static GLint  s_rect_color = -1;

// 직교 투영 행렬 (화면 좌표 → NDC)
// column-major (OpenGL 표준): 각 4개 float가 하나의 열
// ortho(left=0, right=W, bottom=H, top=0, near=-1, far=1)
static float s_proj[16];

// 폰트: wglUseFontBitmaps 로 생성한 GL 디스플레이 리스트
// key = 폰트 크기(px), value = glGenLists 로 받은 기본 ID
static std::map<int, GLuint> s_font_lists;
static std::map<int, HFONT>  s_font_cache;    // measure_text 용 GDI 폰트 캐시 (크기별)
static char   s_font_face[128] = "monogram";  // AddFontResourceEx 후의 face name

// ─── 헬퍼: 셰이더 컴파일 + 링크 ────────────────────────────────────────────
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

// ─── 직교 투영 행렬 계산 ──────────────────────────────────────────────────────
// ortho(0, W, H, 0, -1, 1): 좌상단 (0,0), 우하단 (W,H) 으로 화면 좌표 그대로 사용
// 결과 행렬을 column-major 배열에 저장 (OpenGL glUniformMatrix4fv 호환)
static void build_ortho(float* m, float w, float h)
{
    // Row-major 수식:
    //  [2/W,   0,    0,   -1 ]
    //  [0,   -2/H,   0,    1 ]
    //  [0,     0,   -1,    0 ]
    //  [0,     0,    0,    1 ]
    //
    // Column-major (OpenGL 저장 순서):
    float l = 0.0f, r = w, t = 0.0f, b = h, n = -1.0f, f = 1.0f;
    memset(m, 0, 16 * sizeof(float));
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);   // t < b → 음수 → y축 반전 (화면 아래로)
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
}

// ─── renderer_init ────────────────────────────────────────────────────────────
void renderer_init(int screen_w, int screen_h)
{
    s_screen_w = screen_w;
    s_screen_h = screen_h;

    build_ortho(s_proj, (float)screen_w, (float)screen_h);

    // ── Rect 셰이더 & VAO/VBO ────────────────────────────────────────────────
    s_rect_prog  = link_program(kRectVert, kRectFrag);
    s_rect_proj  = glGetUniformLocation(s_rect_prog, "u_proj");
    s_rect_color = glGetUniformLocation(s_rect_prog, "u_color");

    // VAO: "이 VBO를 어떻게 해석할지" 설정 묶음.
    // 한번 glBindVertexArray + glVertexAttribPointer 하면 이후엔 VAO만 바인딩하면 됨
    glGenVertexArrays(1, &s_rect_vao);
    glBindVertexArray(s_rect_vao);

    // VBO: GPU 메모리에 꼭짓점 데이터를 올려둘 버퍼.
    // GL_DYNAMIC_DRAW: 매 draw call마다 내용이 바뀔 것임을 드라이버에 힌트
    glGenBuffers(1, &s_rect_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // 꼭짓점 레이아웃: location=0, 2개의 float (x, y), stride=8바이트, offset=0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void renderer_begin(Color bg)
{
    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, bg.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_end()
{
    // 현재는 빈 함수. SwapBuffers는 platform_end_frame() 에서.
}

// ─── renderer_shutdown ────────────────────────────────────────────────────────
// GL 리소스 해제. platform_shutdown() 전에 호출해야 함 (GL 컨텍스트가 유효한 동안).
void renderer_shutdown()
{
    // 폰트 디스플레이 리스트 해제
    for (auto& [size, base] : s_font_lists)
    {
        glDeleteLists(base, 96);
    }
    s_font_lists.clear();

    // GDI 폰트 캐시 해제
    for (auto& [size, hfont] : s_font_cache)
    {
        DeleteObject(hfont);
    }
    s_font_cache.clear();

    // 셰이더 프로그램 해제
    if (s_rect_prog) { glDeleteProgram(s_rect_prog); s_rect_prog = 0; }

    // VBO/VAO 해제
    if (s_rect_vbo) { glDeleteBuffers(1, &s_rect_vbo); s_rect_vbo = 0; }
    if (s_rect_vao) { glDeleteVertexArrays(1, &s_rect_vao); s_rect_vao = 0; }
}

// ─── renderer_load_font ───────────────────────────────────────────────────────
// TTF 파일을 GDI에 등록하고, face name을 기억해둡니다.
// 실제 GL 디스플레이 리스트는 draw_text에서 크기별로 lazy하게 생성합니다.
void renderer_load_font(const char* path)
{
    // GDI에 프라이빗 폰트로 등록 (시스템에 설치하지 않아도 사용 가능)
    int added = AddFontResourceExA(path, FR_PRIVATE, nullptr);
    if (added == 0)
    {
        fprintf(stderr, "[FONT] AddFontResourceEx failed for: %s\n", path);
        fprintf(stderr, "[FONT] Falling back to Courier New\n");
        strncpy(s_font_face, "Courier New", sizeof(s_font_face) - 1);
    }
    else
    {
        // monogram.ttf 의 face name은 "monogram"
        strncpy(s_font_face, "monogram", sizeof(s_font_face) - 1);
    }
}

// ─── 폰트 리스트 캐시 ────────────────────────────────────────────────────────
// 크기별로 wglUseFontBitmaps를 lazy하게 호출해 GL 디스플레이 리스트를 만듭니다.
// draw_text 호출 시 해당 크기의 리스트가 없으면 여기서 생성.
static GLuint get_font_list(int size)
{
    auto it = s_font_lists.find(size);
    if (it != s_font_lists.end()) return it->second;

    // 이 크기의 GDI 폰트 생성
    HFONT hfont = CreateFontA(
        size,                    // 높이 (픽셀)
        0,                       // 폭 (0 = 자동)
        0, 0,                    // 기울기 없음
        FW_NORMAL,               // 굵기: 보통
        FALSE, FALSE, FALSE,     // 이탤릭, 밑줄, 취소선 없음
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        s_font_face              // face name: "monogram" 또는 "Courier New"
    );

    if (!hfont)
    {
        fprintf(stderr, "[FONT] CreateFont failed for size %d\n", size);
        return 0;
    }

    // 현재 GL 컨텍스트의 DC에 폰트를 선택하고 디스플레이 리스트 생성
    HDC hdc = (HDC)platform_get_hdc();
    HFONT old_font = (HFONT)SelectObject(hdc, hfont);

    GLuint base = glGenLists(96);  // ASCII 32~127 = 96개 문자
    // wglUseFontBitmaps: 선택된 GDI 폰트로 각 문자의 GL 비트맵 디스플레이 리스트 생성
    // 학습 포인트: 이 함수가 내부에서 GDI로 각 글리프를 렌더링해 GL에 업로드함
    wglUseFontBitmapsA(hdc, 32, 96, base);

    SelectObject(hdc, old_font);
    DeleteObject(hfont);

    s_font_lists[size] = base;
    return base;
}

// ─── draw_rect ───────────────────────────────────────────────────────────────
// 학습 포인트:
//   DrawRectangle(x, y, w, h, color) 한 줄이 아래 과정을 수행합니다.
//   GPU는 삼각형만 이해합니다. 사각형 = 삼각형 2개 = 꼭짓점 6개.
//
//   꼭짓점 배치 (반시계 방향 = OpenGL 기본 앞면):
//   (x,y) ─────── (x+w, y)
//     │    △ △       │
//     │  △   △       │
//     │    △ △       │
//   (x,y+h) ─── (x+w, y+h)
//
//   Triangle 1: (x,y), (x,y+h), (x+w,y+h)
//   Triangle 2: (x,y), (x+w,y+h), (x+w,y)
void draw_rect(int x, int y, int w, int h, Color c)
{
    float fx = (float)x, fy = (float)y;
    float fw = (float)w, fh = (float)h;

    // 6개 꼭짓점 (x,y 각 2 float = 12 float 총)
    float verts[12] = {
        fx,      fy,        // 좌상
        fx,      fy + fh,   // 좌하
        fx + fw, fy + fh,   // 우하
        fx,      fy,        // 좌상
        fx + fw, fy + fh,   // 우하
        fx + fw, fy,        // 우상
    };

    glUseProgram(s_rect_prog);
    glUniformMatrix4fv(s_rect_proj, 1, GL_FALSE, s_proj);
    glUniform4f(s_rect_color,
        c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);

    glBindVertexArray(s_rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_rect_vbo);
    // GL_DYNAMIC_DRAW로 선언했으므로 매 호출마다 데이터를 교체해도 성능 문제 없음
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c)
{
    // 반지름 계산: roundness(0~1) × min(w,h)/2
    float minDim = (float)(w < h ? w : h);
    float r = roundness * minDim * 0.5f;
    if (r < 1.0f) { draw_rect(x, y, w, h, c); return; }

    float fx = (float)x, fy = (float)y, fw = (float)w, fh = (float)h;

    // 코너당 세그먼트 수 (8 = 충분히 부드러움, 총 꼭짓점 합리적)
    const int N_SEG = 8;
    // 3개 사각형(18) + 4개 코너 아크(4 * N_SEG * 3 = 96) = 114 꼭짓점
    const int MAX_VERTS = 18 + 4 * N_SEG * 3;
    float verts[MAX_VERTS * 2];  // x,y 쌍
    int vi = 0;

    // 꼭짓점 추가 헬퍼
    auto V = [&](float px, float py) { verts[vi++] = px; verts[vi++] = py; };

    // 사각형 추가 헬퍼 (삼각형 2개 = 꼭짓점 6개)
    auto Rect = [&](float rx, float ry, float rw, float rh) {
        V(rx, ry);       V(rx, ry+rh);       V(rx+rw, ry+rh);
        V(rx, ry);       V(rx+rw, ry+rh);    V(rx+rw, ry);
    };

    //  ┌─────────────────────────────────────────────────┐
    //  │  분해 구조:                                      │
    //  │                                                   │
    //  │    ╭──── center strip (세로 전체, 좌우 r 인셋) ──╮ │
    //  │  left│                                        │right│
    //  │  strip│                                      │strip│
    //  │    ╰──────────────────────────────────────────╯ │
    //  │  4개 코너 = quarter-circle 삼각형 팬             │
    //  └─────────────────────────────────────────────────┘
    Rect(fx + r, fy,         fw - 2*r, fh);         // 중앙 가로 스트립
    Rect(fx,     fy + r,     r,        fh - 2*r);   // 좌측 세로 스트립
    Rect(fx + fw - r, fy + r, r,       fh - 2*r);   // 우측 세로 스트립

    // 코너 아크 (삼각형 팬을 삼각형 리스트로)
    // 화면 좌표계: x→오른쪽, y→아래쪽
    // cos(a) → x축, sin(a) → y축(아래 양수)
    //   angle 0     → (r, 0)    오른쪽
    //   angle PI/2  → (0, r)    아래
    //   angle PI    → (-r, 0)   왼쪽
    //   angle 3PI/2 → (0, -r)   위
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

    Corner(fx + r,      fy + r,      PI);         // 좌상: PI → 3PI/2
    Corner(fx + fw - r, fy + r,      PI * 1.5f);  // 우상: 3PI/2 → 2PI
    Corner(fx + fw - r, fy + fh - r, 0.0f);       // 우하: 0 → PI/2
    Corner(fx + r,      fy + fh - r, PI * 0.5f);  // 좌하: PI/2 → PI

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

// ─── draw_text ────────────────────────────────────────────────────────────────
// GDI 폰트 비트맵을 GL 디스플레이 리스트로 변환해 렌더링.
//
// 학습 포인트:
//   wglUseFontBitmaps / glCallLists 는 OpenGL 1.x 호환 모드(compatibility profile)
//   에서만 동작합니다. 현재 컨텍스트가 compatibility profile이므로 사용 가능.
//
//   glWindowPos2i: 래스터 위치를 윈도우 좌표로 직접 설정 (y축 반전 주의!)
//     GL의 y=0은 화면 아래쪽 → screen_h - y - size 변환 필요
void draw_text(const char* text, int x, int y, int size, Color c)
{
    if (!text || !text[0]) return;
    GLuint base = get_font_list(size);
    if (!base) return;

    // draw_rect가 남겨놓은 셰이더 프로그램을 해제해야 고정 파이프라인(glColor/glCallLists)이 작동
    glUseProgram(0);

    // 색상을 glWindowPos2i 보다 먼저 설정해야 함.
    // glWindowPos2i가 호출 시점의 current color를 raster color로 스냅샷하고,
    // glBitmap(디스플레이 리스트 내부)은 raster color를 사용하기 때문.
    glColor4ub(c.r, c.g, c.b, c.a);

    // GL은 y=0이 화면 아래쪽. glWindowPos2i 는 그 좌표계를 씀.
    // 화면 좌표 y(위쪽 기준) → GL 좌표: screen_h - y - line_height
    glWindowPos2i(x, s_screen_h - y - size);

    // glCallLists: base + (char - 32) 번 디스플레이 리스트를 문자별로 호출
    glListBase(base - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
}

// ─── 캐시된 GDI 폰트 ─────────────────────────────────────────────────────────
// measure_text()가 매 호출마다 CreateFont/DeleteObject 하던 것을 크기별 캐시로 교체.
static HFONT get_cached_font(int size)
{
    auto it = s_font_cache.find(size);
    if (it != s_font_cache.end()) return it->second;

    HFONT hfont = CreateFontA(
        size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        s_font_face);

    if (hfont) s_font_cache[size] = hfont;
    return hfont;
}

// ─── measure_text ─────────────────────────────────────────────────────────────
// GDI의 GetTextExtentPoint32 로 텍스트 폭을 측정합니다.
// raylib::MeasureTextEx() 대체.
int measure_text(const char* text, int size)
{
    if (!text || !text[0]) return 0;

    HFONT hfont = get_cached_font(size);
    if (!hfont) return (int)(strlen(text) * size * 0.6f); // 추정값 폴백

    HDC hdc = (HDC)platform_get_hdc();
    HFONT old = (HFONT)SelectObject(hdc, hfont);
    SIZE sz;
    GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
    SelectObject(hdc, old);
    return sz.cx;
}
