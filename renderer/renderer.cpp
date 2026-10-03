// renderer/renderer.cpp — OpenGL 3.3 Core 2D 렌더러
//
// 게임 코드가 부르는 draw_* 는 즉시 그리지 않고 정점을 큐에 쌓는다.
// 텍스처 변경·프레임 끝·아틀라스 재활용 경계에서 쌓인 정점을 제출한다.
//
// 좌표계는 논리 픽셀(좌상단 원점)이고, NDC 변환은 vertex 셰이더가 한다.
// 그래서 이 파일에는 투영 행렬이 없다 — u_screen 하나로 충분하다.

#include "renderer.h"
#include "gl_internal.h"
#include "gl_shaders.h"
#include "image.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

// ─── 상태 ─────────────────────────────────────────────────────────────────────

static int s_screen_w = 0;
static int s_screen_h = 0;
static int s_view_ox  = 0;
static int s_view_oy  = 0;
static float s_render_scale = 1.0f;

static GLuint s_prog        = 0;
static GLuint s_vao         = 0;
static GLuint s_vbo         = 0;
static GLuint s_white       = 0;
static GLint  s_u_screen    = -1;
static GLint  s_u_tex       = -1;

// 정점 하나: pos(2) uv(2) color(4) local(2) half(2) radius(1) channel(1)
static constexpr int kFloatsPerVertex = 14;

static std::vector<float> s_verts;      // 용량 재사용 — capacity를 넘으면 재할당 가능
static GLuint             s_batch_tex = 0;
static bool               s_ready     = false;

// ─── 셰이더 ───────────────────────────────────────────────────────────────────

static GLuint compile_shader(GLenum type, const char* src, const char* label)
{
    GLuint s = gl_CreateShader(type);
    if (!s) {
        std::fprintf(stderr, "[GL] %s shader creation failed.\n", label);
        return 0;
    }
    gl_ShaderSource(s, 1, &src, nullptr);
    gl_CompileShader(s);

    GLint ok = GL_FALSE;
    gl_GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        // 셰이더는 사용자 기계에서 컴파일된다. 드라이버마다 GLSL 프론트엔드가
        // 달라 내 기계에서 통과한 코드가 남의 기계에서 막힐 수 있으므로,
        // 로그를 삼키지 않고 그대로 보여준다.
        GLint len = 0;
        gl_GetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? (size_t)len : 1, '\0');
        gl_GetShaderInfoLog(s, (GLsizei)log.size(), nullptr, log.data());
        std::fprintf(stderr, "[GL] %s shader compile failed:\n%s\n", label, log.data());
        gl_DeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(const char* vs_src, const char* fs_src)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src, "vertex");
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, "fragment");
    if (!vs || !fs) {
        if (vs) gl_DeleteShader(vs);
        if (fs) gl_DeleteShader(fs);
        return 0;
    }

    GLuint p = gl_CreateProgram();
    if (!p) {
        std::fprintf(stderr, "[GL] program creation failed.\n");
        gl_DeleteShader(vs);
        gl_DeleteShader(fs);
        return 0;
    }
    gl_AttachShader(p, vs);
    gl_AttachShader(p, fs);
    gl_LinkProgram(p);

    GLint ok = GL_FALSE;
    gl_GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl_GetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? (size_t)len : 1, '\0');
        gl_GetProgramInfoLog(p, (GLsizei)log.size(), nullptr, log.data());
        std::fprintf(stderr, "[GL] program link failed:\n%s\n", log.data());
        gl_DeleteProgram(p);
        p = 0;
    }

    // 삭제를 요청한다. 성공한 프로그램에 붙어 있는 셰이더의 실제 삭제는
    // 프로그램 삭제로 연결이 해제될 때까지 지연되며, 링크된 실행 코드는 유지된다.
    gl_DeleteShader(vs);
    gl_DeleteShader(fs);
    return p;
}

// ─── 배처 ─────────────────────────────────────────────────────────────────────

static void push_vertex(float x, float y, float u, float v, Color c,
                        float lx, float ly, float hw, float hh,
                        float radius, float channel)
{
    s_verts.insert(s_verts.end(), {
        x, y, u, v,
        c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f,
        lx, ly, hw, hh, radius, channel
    });
}

// 이 배처는 draw마다 한 텍스처를 바인딩한다. 텍스처가 바뀌기 전에
// 쌓인 정점을 먼저 그려 같은 배치의 텍스처 해석을 유지한다.
static void ensure_texture(GLuint tex)
{
    if (s_batch_tex != tex) {
        glb_flush();
        s_batch_tex = tex;
    }
}

void glb_flush()
{
    if (!s_ready || s_verts.empty()) return;

    gl_BindBuffer(GL_ARRAY_BUFFER, s_vbo);
    gl_BufferData(GL_ARRAY_BUFFER,
                  (GLsizeiptr)(s_verts.size() * sizeof(float)),
                  s_verts.data(), GL_STREAM_DRAW);

    gl_ActiveTexture(GL_TEXTURE0);
    gl_BindTexture(GL_TEXTURE_2D, s_batch_tex ? s_batch_tex : s_white);

    gl_BindVertexArray(s_vao);
    gl_DrawArrays(GL_TRIANGLES, 0,
                  (GLsizei)(s_verts.size() / kFloatsPerVertex));

    s_verts.clear();
}

// The texture must stay alive until CPU vertices that name it are submitted.
// Forget the name as well: GL may reuse it for a different texture later.
void glb_before_texture_delete(GLuint tex)
{
    if (tex && s_batch_tex == tex) {
        glb_flush();
        s_batch_tex = 0;
    }
}

void glb_rect(GLuint tex,
              float x, float y, float w, float h,
              float u0, float v0, float u1, float v1,
              Color c, float radius, float channel)
{
    if (!s_ready || w <= 0.0f || h <= 0.0f || c.a == 0) return;

    x += (float)s_view_ox;
    y += (float)s_view_oy;

    // 화면 밖은 정점을 만들지 않는다. GPU 가 어차피 버리지만 대역폭이 아깝다.
    if (x + w <= 0.0f || y + h <= 0.0f ||
        x >= (float)s_screen_w || y >= (float)s_screen_h) return;

    ensure_texture(tex);

    const float hw = w * 0.5f;
    const float hh = h * 0.5f;

    // TL, TR, BR / TL, BR, BL — 삼각형 두 개
    const float xs[4] = { x,      x + w,  x + w,  x     };
    const float ys[4] = { y,      y,      y + h,  y + h };
    const float us[4] = { u0,     u1,     u1,     u0    };
    const float vs[4] = { v0,     v0,     v1,     v1    };
    const float lxs[4] = { -hw,    hw,     hw,    -hw   };
    const float lys[4] = { -hh,   -hh,     hh,     hh   };

    const int order[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; ++i) {
        const int k = order[i];
        push_vertex(xs[k], ys[k], us[k], vs[k], c,
                    lxs[k], lys[k], hw, hh, radius, channel);
    }
}

// px/py와 uu/vv는 같은 꼭짓점 순서(TL, TR, BR, BL)를 공유한다.
// 고정 대각선 0-2를 쓰므로 호출자는 뒤틀리거나 교차하지 않는 볼록 사각형을 준다.
void glb_quad(GLuint tex,
              const float px[4], const float py[4],
              const float uu[4], const float vv[4],
              Color c, float channel)
{
    if (!s_ready || c.a == 0) return;
    ensure_texture(tex);

    const int order[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; ++i) {
        const int k = order[i];
        push_vertex(px[k] + (float)s_view_ox, py[k] + (float)s_view_oy,
                    uu[k], vv[k], c, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, channel);
    }
}

GLuint glb_white_texture()   { return s_white; }
int    glb_screen_width()    { return s_screen_w; }
int    glb_screen_height()   { return s_screen_h; }
float  glb_render_scale()    { return s_render_scale; }

// ─── 공개 API ─────────────────────────────────────────────────────────────────

bool renderer_init(int screen_w, int screen_h)
{
    s_screen_w = screen_w > 0 ? screen_w : 1;
    s_screen_h = screen_h > 0 ? screen_h : 1;
    s_view_ox = s_view_oy = 0;

    if (!gl_load_functions()) {
        std::fprintf(stderr, "[GL] renderer_init aborted.\n");
        return false;
    }

    s_prog = link_program(kQuadVert, kQuadFrag);
    if (!s_prog) {
        std::fprintf(stderr, "[GL] renderer_init aborted: shader program.\n");
        return false;
    }
    s_u_screen = gl_GetUniformLocation(s_prog, "u_screen");
    s_u_tex    = gl_GetUniformLocation(s_prog, "u_tex");

    gl_GenVertexArrays(1, &s_vao);
    gl_BindVertexArray(s_vao);
    gl_GenBuffers(1, &s_vbo);
    gl_BindBuffer(GL_ARRAY_BUFFER, s_vbo);

    const GLsizei stride = kFloatsPerVertex * (GLsizei)sizeof(float);
    struct { GLuint loc; GLint size; size_t offset; } attribs[] = {
        { 0, 2, 0  }, { 1, 2, 2  }, { 2, 4, 4  },
        { 3, 2, 8  }, { 4, 2, 10 }, { 5, 1, 12 }, { 6, 1, 13 },
    };
    for (const auto& a : attribs) {
        gl_VertexAttribPointer(a.loc, a.size, GL_FLOAT, GL_FALSE, stride,
                               (const void*)(a.offset * sizeof(float)));
        gl_EnableVertexAttribArray(a.loc);
    }

    // 단색 도형이 텍스처 없이도 같은 셰이더를 타도록 1x1 흰 픽셀을 둔다.
    const unsigned char white[4] = { 255, 255, 255, 255 };
    gl_GenTextures(1, &s_white);
    gl_BindTexture(GL_TEXTURE_2D, s_white);
    gl_TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // RGB는 straight-alpha 입력으로 합성한다. 저장 alpha에는 같은 계수가 적용되어
    // As*As + Ad*(1-As)가 된다. 투명한 중간 이미지의 source-over에는 별도 설정이 필요하다.
    gl_Enable(GL_BLEND);
    gl_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    s_verts.reserve(4096 * kFloatsPerVertex);
    s_batch_tex = s_white;
    s_ready = true;

    image_init();
    return true;
}

void renderer_begin(Color bg)
{
    if (!s_ready) return;

    // 창이 리사이즈됐으면 표시 영역을 따라간다. 논리 해상도는 그대로 두고
    // 뷰포트만 바꾸므로, 창을 늘려도 UI 좌표계는 한 픽셀도 변하지 않는다.
    // 종횡비가 다른 창에서는 뷰포트가 창보다 작아 가장자리에 여백이 남는다.
    int vx = 0, vy = 0, vw = 0, vh = 0;
    platform_viewport(vx, vy, vw, vh);

    // 플랫폼이 빈 표시 영역을 반환하면 배경 지우기를 건너뛰고 GL 뷰포트를
    // 0x0으로 설정한다. 게임 코드는 최소화 여부를 모르고 draw_* 를 부르지만,
    // 그 정점들은 프레임 끝의 glb_flush 가 0x0 뷰포트로 흘려보내고 큐를
    // 비우므로 쌓이지는 않는다. 다만 배처 상태는 여기서 맞춰 둔다 —
    // 그러지 않으면 첫 프레임부터 최소화로 시작했을 때 glUseProgram 을
    // 한 번도 부르지 않은 채 glDrawArrays 에 도달한다.
    if (vw <= 0 || vh <= 0) {
        gl_Viewport(0, 0, 0, 0); // 이전 프레임의 GL 뷰포트를 남기지 않는다.
        gl_UseProgram(s_prog);
        s_verts.clear();
        s_batch_tex = s_white;
        return;
    }
    gl_Viewport(vx, vy, vw, vh);

    // 정수 뷰포트의 반올림 때문에 두 축 배율은 조금 다를 수 있다.
    // 글리프 배율은 세로 높이를 기준으로 정한다.
    s_render_scale = (float)vh / (float)s_screen_h;

    // glClear 는 뷰포트가 아니라 시저 박스를 따른다. glViewport 만 좁혀 놓고
    // 지우면 레터박스 여백까지 배경색으로 칠해져 여백과 게임 화면의 경계가
    // 사라진다. 그래서 두 번 지운다 — 창 전체를 검게, 뷰포트 안만 배경색으로.
    gl_Disable(GL_SCISSOR_TEST);
    gl_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl_Clear(GL_COLOR_BUFFER_BIT);

    gl_Enable(GL_SCISSOR_TEST);
    gl_Scissor(vx, vy, vw, vh);
    gl_ClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, 1.0f);
    gl_Clear(GL_COLOR_BUFFER_BIT);

    // 시저는 켠 채로 둔다. 논리 좌표를 벗어나게 그리는 코드가 있어도
    // 여백을 침범하지 못하게 하는 안전장치다.

    gl_UseProgram(s_prog);
    gl_Uniform2f(s_u_screen, (float)s_screen_w, (float)s_screen_h);
    gl_Uniform1i(s_u_tex, 0);

    s_verts.clear();
    s_batch_tex = s_white;
}

void renderer_set_view_offset(int dx, int dy)
{
    // glb_rect/glb_quad bake the offset into each submitted CPU vertex.
    // Different baked offsets can share one ordered batch; no GPU state changes.
    s_view_ox = dx;
    s_view_oy = dy;
}

void renderer_end()
{
    if (!s_ready) return;
    glb_flush();
    platform_present();
}

void renderer_shutdown()
{
    if (s_ready) {
        glb_flush(); // Submit while every referenced texture/program/buffer is alive.
        image_shutdown();
        renderer_text_shutdown();
        if (s_white) gl_DeleteTextures(1, &s_white);
        if (s_vbo)   gl_DeleteBuffers(1, &s_vbo);
        if (s_vao)   gl_DeleteVertexArrays(1, &s_vao);
        if (s_prog)  gl_DeleteProgram(s_prog);
    }
    s_white = s_vbo = s_vao = s_prog = 0;
    s_verts.clear();
    s_verts.shrink_to_fit();
    s_ready = false;
}

void draw_rect(int x, int y, int w, int h, Color c)
{
    glb_rect(s_white, (float)x, (float)y, (float)w, (float)h,
             0.0f, 0.0f, 1.0f, 1.0f, c, 0.0f, 0.0f);
}

void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c)
{
    if (w <= 0 || h <= 0 || !std::isfinite(roundness)) return;
    if (roundness < 0.0f) roundness = 0.0f;
    if (roundness > 1.0f) roundness = 1.0f;
    const float shorter = (float)(w < h ? w : h);
    const float radius  = roundness * 0.5f * shorter;

    // 논리 반지름이 1 미만이면 각진 사각형으로 근사한다.
    // 작은 양수 반지름의 SDF 결과와 수학적으로 같은 것은 아니다.
    glb_rect(s_white, (float)x, (float)y, (float)w, (float)h,
             0.0f, 0.0f, 1.0f, 1.0f, c,
             radius < 1.0f ? 0.0f : radius, 0.0f);
}
