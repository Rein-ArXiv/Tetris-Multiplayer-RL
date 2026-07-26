// renderer/text_gl.cpp — stb_truetype 래스터화 + GPU 글리프 아틀라스
//
// 글자 모양을 만드는 일은 여전히 CPU 가 한다. stb_truetype 가 TTF 아웃라인을
// 8비트 coverage 비트맵으로 그려 주는데, GPU 에는 그런 기능이 없기 때문이다.
// 바뀐 것은 그 비트맵을 어디에 두느냐다.
//
//   이전: 비트맵을 CPU 메모리에 캐시하고 픽셀마다 프레임버퍼에 합성
//   지금: 비트맵을 한 장의 R8 텍스처(아틀라스)에 올리고, 그릴 때는
//         그 텍스처의 일부를 가리키는 사각형 하나만 배처에 넣는다
//
// 아틀라스를 쓰는 이유는 텍스처 교체가 draw call 을 끊기 때문이다. 글자마다
// 텍스처가 따로면 "Game Over" 한 줄에 draw call 이 9번 나간다. 한 장에 모아
// 두면 화면의 모든 글자가 한 번에 나간다.

#include "renderer.h"
#include "gl_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

// 아틀라스 한 변. 1024² R8 = 1 MB. 이 게임이 쓰는 글꼴 크기 종류가 유한해서
// (UI 14~64px 남짓) 실사용에서 넘친 적이 없다. 넘치면 아래 pack_glyph 가
// 경고를 찍고 그 글자를 빈 칸으로 돌려준다 — 조용히 깨지지 않게 하려는 것이다.
static constexpr int kAtlasDim = 1024;

struct Glyph {
    int   w = 0, h = 0;          // 비트맵 크기 (픽셀)
    int   xoff = 0, yoff = 0;    // 펜 위치 기준 오프셋
    float advance = 0.0f;
    float u0 = 0.0f, v0 = 0.0f;  // 아틀라스 안에서의 위치
    float u1 = 0.0f, v1 = 0.0f;
};

static stbtt_fontinfo s_font{};
static std::vector<uint8_t> s_ttf;
static std::unordered_map<uint64_t, Glyph> s_cache;
static bool   s_font_ok = false;

static GLuint s_atlas    = 0;
static int    s_pen_x    = 0;   // shelf packing 커서
static int    s_pen_y    = 0;
static int    s_row_h    = 0;

static uint32_t utf8_next(const char** text)
{
    const uint8_t* s = reinterpret_cast<const uint8_t*>(*text);
    if (!s[0]) return 0;
    uint32_t cp = 0;
    int count = 0;
    if (s[0] < 0x80) {
        cp = s[0]; count = 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = s[0] & 0x1F; count = 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = s[0] & 0x0F; count = 3;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = s[0] & 0x07; count = 4;
    } else {
        ++*text;
        return 0xFFFD;
    }
    for (int i = 1; i < count; ++i) {
        if ((s[i] & 0xC0) != 0x80) {
            ++*text;
            return 0xFFFD;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *text += count;
    return cp;
}

static void ensure_atlas()
{
    if (s_atlas) return;
    gl_GenTextures(1, &s_atlas);
    gl_BindTexture(GL_TEXTURE_2D, s_atlas);
    // 채널이 하나뿐이라 기본 4바이트 정렬 규칙이 맞지 않는다. 이걸 빠뜨리면
    // 폭이 4의 배수가 아닌 글자가 비스듬히 밀려 보인다.
    gl_PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    std::vector<uint8_t> zero((size_t)kAtlasDim * kAtlasDim, 0);
    gl_TexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasDim, kAtlasDim, 0,
                  GL_RED, GL_UNSIGNED_BYTE, zero.data());
    // 글리프는 등배로 그리므로 LINEAR 가 오히려 번진다.
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    s_pen_x = s_pen_y = s_row_h = 0;
}

// shelf packing: 왼쪽에서 오른쪽으로 채우다 폭이 모자라면 다음 줄로 내린다.
// 최적 패킹은 아니지만 글리프 높이가 크기별로 비슷해서 낭비가 크지 않다.
static bool pack_glyph(const uint8_t* bitmap, int w, int h, Glyph& out)
{
    if (w <= 0 || h <= 0) return true;   // 공백 문자 — 자리를 차지하지 않는다

    if (s_pen_x + w > kAtlasDim) {       // 줄 바꿈
        s_pen_x = 0;
        s_pen_y += s_row_h + 1;
        s_row_h = 0;
    }
    if (s_pen_y + h > kAtlasDim) {
        std::fprintf(stderr, "[text] glyph atlas full (%dx%d)\n", kAtlasDim, kAtlasDim);
        return false;
    }

    gl_BindTexture(GL_TEXTURE_2D, s_atlas);
    gl_PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_TexSubImage2D(GL_TEXTURE_2D, 0, s_pen_x, s_pen_y, w, h,
                     GL_RED, GL_UNSIGNED_BYTE, bitmap);

    out.u0 = (float)s_pen_x / (float)kAtlasDim;
    out.v0 = (float)s_pen_y / (float)kAtlasDim;
    out.u1 = (float)(s_pen_x + w) / (float)kAtlasDim;
    out.v1 = (float)(s_pen_y + h) / (float)kAtlasDim;

    s_pen_x += w + 1;                     // 1픽셀 간격 — 샘플링 번짐 방지
    s_row_h = std::max(s_row_h, h);
    return true;
}

static const Glyph& glyph_for(uint32_t cp, int px)
{
    px = px < 1 ? 1 : px;
    const uint64_t key = (uint64_t(cp) << 32) | uint32_t(px);
    auto found = s_cache.find(key);
    if (found != s_cache.end()) return found->second;

    ensure_atlas();

    Glyph glyph;
    const float scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
    int advance = 0;
    int left_bearing = 0;
    stbtt_GetCodepointHMetrics(&s_font, (int)cp, &advance, &left_bearing);
    glyph.advance = (float)advance * scale;

    unsigned char* bitmap = stbtt_GetCodepointBitmap(
        &s_font, scale, scale, (int)cp,
        &glyph.w, &glyph.h, &glyph.xoff, &glyph.yoff);
    if (bitmap && glyph.w > 0 && glyph.h > 0) {
        if (!pack_glyph(bitmap, glyph.w, glyph.h, glyph)) {
            glyph.w = glyph.h = 0;        // 자리 없음 — 그리지 않는다
        }
    }
    if (bitmap) stbtt_FreeBitmap(bitmap, nullptr);
    return s_cache.emplace(key, glyph).first->second;
}

void renderer_load_font(const char* path)
{
    s_font_ok = false;
    s_cache.clear();
    s_ttf.clear();
    // 폰트가 바뀌면 아틀라스 내용이 의미를 잃으므로 커서를 되감는다.
    s_pen_x = s_pen_y = s_row_h = 0;
    if (!path || !*path) return;

    FILE* file = std::fopen(path, "rb");
    if (!file) {
        std::fprintf(stderr, "[text] font open failed: %s\n", path);
        return;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        std::fprintf(stderr, "[text] font empty: %s\n", path);
        return;
    }
    s_ttf.resize((size_t)size);
    const size_t read = std::fread(s_ttf.data(), 1, s_ttf.size(), file);
    std::fclose(file);
    if (read != s_ttf.size()) {
        s_ttf.clear();
        std::fprintf(stderr, "[text] font read failed: %s\n", path);
        return;
    }

    const int offset = stbtt_GetFontOffsetForIndex(s_ttf.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&s_font, s_ttf.data(), offset)) {
        s_ttf.clear();
        std::fprintf(stderr, "[text] invalid TTF: %s\n", path);
        return;
    }
    s_font_ok = true;
}

int measure_text(const char* text, int size)
{
    if (!text || !*text || !s_font_ok) return 0;
    const int px = size < 1 ? 1 : size;
    float line_width = 0.0f;
    float max_width = 0.0f;
    uint32_t previous = 0;
    for (const char* p = text; *p;) {
        const uint32_t cp = utf8_next(&p);
        if (cp == '\n') {
            max_width = std::max(max_width, line_width);
            line_width = 0.0f;
            previous = 0;
            continue;
        }
        const float scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
        if (previous)
            line_width += stbtt_GetCodepointKernAdvance(
                &s_font, (int)previous, (int)cp) * scale;
        line_width += glyph_for(cp, px).advance;
        previous = cp;
    }
    max_width = std::max(max_width, line_width);
    return (int)std::floor(max_width + 0.5f);
}

void draw_text(const char* text, int x, int y, int size, Color color)
{
    if (!text || !*text || !s_font_ok || color.a == 0) return;
    const int px = size < 1 ? 1 : size;
    const float scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&s_font, &ascent, &descent, &line_gap);
    const float baseline0 = (float)y + (float)ascent * scale;
    const float line_advance = (float)(ascent - descent + line_gap) * scale;

    float pen_x = (float)x;
    float baseline = baseline0;
    uint32_t previous = 0;
    for (const char* p = text; *p;) {
        const uint32_t cp = utf8_next(&p);
        if (cp == '\n') {
            pen_x = (float)x;
            baseline += line_advance;
            previous = 0;
            continue;
        }
        if (previous)
            pen_x += stbtt_GetCodepointKernAdvance(
                &s_font, (int)previous, (int)cp) * scale;

        const Glyph& glyph = glyph_for(cp, px);
        if (glyph.w > 0 && glyph.h > 0) {
            const float gx = std::floor(pen_x + (float)glyph.xoff);
            const float gy = std::floor(baseline + (float)glyph.yoff);
            // channel = 1 — 셰이더가 R8 의 r 을 알파로 읽고 color 를 곱한다.
            glb_rect(s_atlas, gx, gy, (float)glyph.w, (float)glyph.h,
                     glyph.u0, glyph.v0, glyph.u1, glyph.v1,
                     color, 0.0f, 1.0f);
        }
        pen_x += glyph.advance;
        previous = cp;
    }
}

void renderer_text_shutdown()
{
    if (s_atlas) {
        gl_DeleteTextures(1, &s_atlas);
        s_atlas = 0;
    }
    s_cache.clear();
    s_ttf.clear();
    s_pen_x = s_pen_y = s_row_h = 0;
    s_font_ok = false;
}
