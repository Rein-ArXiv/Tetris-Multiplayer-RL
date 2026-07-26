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
//
// 해상도에 대해: 사각형은 정점 좌표가 실수라 창을 4K 로 키워도 GPU 가 그
// 해상도로 다시 래스터화한다 — 저절로 선명하다. 글자는 그렇지 않다. 한 번
// 구운 비트맵을 확대하면 그 배율만큼 뭉갠다. 그래서 여기서는 배치는 논리
// 좌표로 하되, **굽는 크기만** 화면 배율을 곱해 키운다. 22px 글자를 3.4배
// 창에서 보면 실제로는 75px 로 구워 22px 자리에 그린다.

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

// 아틀라스 한 변. 2048² R8 = 4 MB 를 노린다. 1024 로도 논리 해상도에서는
// 남지만, 4K 창에서는 같은 글자를 3~4배 크기로 굽기 때문에 금방 찬다.
// 드라이버가 허용하는 상한이 더 낮을 수 있어 실제 값은 ensure_atlas 에서
// GL_MAX_TEXTURE_SIZE 와 비교해 정한다.
static constexpr int kAtlasWanted = 2048;
static int s_atlas_dim = kAtlasWanted;

// 굽는 크기를 1/8 단위로 반올림한다. 창을 드래그로 늘리는 동안 배율이
// 연속으로 변하는데, 그때마다 새 크기로 다시 구우면 아틀라스가 순식간에
// 찬다. 눈에 안 보이는 차이를 같은 크기로 묶어 재굽기를 줄인다.
static constexpr float kScaleQuantum = 8.0f;

struct Glyph {
    int   bw = 0, bh = 0;        // 구워진 비트맵 크기 (실제 화면 픽셀)
    float w = 0.0f, h = 0.0f;    // 그릴 크기 (논리 픽셀)
    float xoff = 0.0f;           // 펜 위치 기준 오프셋 (논리 픽셀)
    float yoff = 0.0f;
    float advance = 0.0f;        // 다음 글자까지 (논리 픽셀)
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

    GLint max_dim = 0;
    gl_GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_dim);
    s_atlas_dim = (max_dim > 0 && max_dim < kAtlasWanted) ? (int)max_dim
                                                          : kAtlasWanted;

    gl_GenTextures(1, &s_atlas);
    gl_BindTexture(GL_TEXTURE_2D, s_atlas);
    // 채널이 하나뿐이라 기본 4바이트 정렬 규칙이 맞지 않는다. 이걸 빠뜨리면
    // 폭이 4의 배수가 아닌 글자가 비스듬히 밀려 보인다.
    gl_PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    std::vector<uint8_t> zero((size_t)s_atlas_dim * s_atlas_dim, 0);
    gl_TexImage2D(GL_TEXTURE_2D, 0, GL_R8, s_atlas_dim, s_atlas_dim, 0,
                  GL_RED, GL_UNSIGNED_BYTE, zero.data());
    // 굽는 크기를 1/8 단위로 반올림하므로 화면 픽셀과 텍셀이 정확히 1:1 은
    // 아니다 (최대 6% 어긋난다). NEAREST 로 두면 그 어긋남이 글자 획 굵기가
    // 들쭉날쭉해지는 형태로 보인다. LINEAR 가 그 차이를 흡수한다.
    // 글리프 사이에 1픽셀 빈 줄을 두므로 이웃 글자가 번져 들어오지 않는다.
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    s_pen_x = s_pen_y = s_row_h = 0;
}

// shelf packing: 왼쪽에서 오른쪽으로 채우다 폭이 모자라면 다음 줄로 내린다.
// 최적 패킹은 아니지만 글리프 높이가 크기별로 비슷해서 낭비가 크지 않다.
static bool pack_glyph(const uint8_t* bitmap, int w, int h, Glyph& out)
{
    if (w <= 0 || h <= 0) return true;   // 공백 문자 — 자리를 차지하지 않는다
    if (w > s_atlas_dim || h > s_atlas_dim) return false;  // 한 장에 안 들어가는 글자

    if (s_pen_x + w > s_atlas_dim) {       // 줄 바꿈
        s_pen_x = 0;
        s_pen_y += s_row_h + 1;
        s_row_h = 0;
    }
    if (s_pen_y + h > s_atlas_dim) {
        // 가득 찼다. 예전 크기로 구운 글자들이 대부분이므로 (창 크기가
        // 바뀌면 이전 배율 비트맵은 다시 안 쓰인다) 통째로 버리고 처음부터
        // 다시 채운다. 개별 항목을 쫓아내는 LRU 보다 단순하고, 실제로는
        // 창 크기를 크게 바꿀 때 한 번씩만 일어난다.
        //
        // 버리기 전에 배치를 비운다. 이미 큐에 들어간 글자들의 UV 는 지금
        // 아틀라스 내용을 가리키는데, 비우지 않고 덮어쓰면 그 글자들이
        // 새로 구운 다른 글자의 그림으로 그려진다.
        glb_flush();
        s_cache.clear();
        s_pen_x = s_pen_y = s_row_h = 0;
    }

    gl_BindTexture(GL_TEXTURE_2D, s_atlas);
    gl_PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_TexSubImage2D(GL_TEXTURE_2D, 0, s_pen_x, s_pen_y, w, h,
                     GL_RED, GL_UNSIGNED_BYTE, bitmap);

    out.u0 = (float)s_pen_x / (float)s_atlas_dim;
    out.v0 = (float)s_pen_y / (float)s_atlas_dim;
    out.u1 = (float)(s_pen_x + w) / (float)s_atlas_dim;
    out.v1 = (float)(s_pen_y + h) / (float)s_atlas_dim;

    s_pen_x += w + 1;                     // 1픽셀 간격 — 샘플링 번짐 방지
    s_row_h = std::max(s_row_h, h);
    return true;
}

static Glyph glyph_for(uint32_t cp, int px)
{
    px = px < 1 ? 1 : px;

    // 실제로 구울 크기. 논리 크기 × 화면 배율을 1/8 단위로 반올림한다.
    const float scale_q = std::max(
        1.0f, std::round(glb_render_scale() * kScaleQuantum) / kScaleQuantum);
    const int dev_px = std::max(1, (int)std::lround((float)px * scale_q));

    // 캐시 키에 굽는 크기까지 넣는다. 같은 22px 글자라도 창 배율이 다르면
    // 다른 비트맵이므로 따로 보관해야 한다.
    const uint64_t key = (uint64_t(cp) << 32) |
                         (uint64_t(uint16_t(px)) << 16) | uint16_t(dev_px);
    auto found = s_cache.find(key);
    if (found != s_cache.end()) return found->second;

    ensure_atlas();

    Glyph glyph;

    // 배치용 메트릭은 **논리 크기 기준**으로 낸다. 창을 늘렸다고 글자 간격이
    // 달라지면 버튼 안의 텍스트가 넘치는 식으로 레이아웃이 흔들린다.
    const float layout_scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
    int advance = 0;
    int left_bearing = 0;
    stbtt_GetCodepointHMetrics(&s_font, (int)cp, &advance, &left_bearing);
    glyph.advance = (float)advance * layout_scale;

    // 비트맵만 확대된 크기로 굽는다.
    const float bake_scale = stbtt_ScaleForPixelHeight(&s_font, (float)dev_px);
    int bx = 0, by = 0;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(
        &s_font, bake_scale, bake_scale, (int)cp,
        &glyph.bw, &glyph.bh, &bx, &by);

    // 화면 픽셀 단위로 나온 크기/오프셋을 논리 단위로 되돌린다.
    const float inv = (float)px / (float)dev_px;
    glyph.w    = (float)glyph.bw * inv;
    glyph.h    = (float)glyph.bh * inv;
    glyph.xoff = (float)bx * inv;
    glyph.yoff = (float)by * inv;

    if (bitmap && glyph.bw > 0 && glyph.bh > 0) {
        if (!pack_glyph(bitmap, glyph.bw, glyph.bh, glyph)) {
            glyph.bw = glyph.bh = 0;      // 자리 없음 — 그리지 않는다
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

        const Glyph glyph = glyph_for(cp, px);
        if (glyph.bw > 0 && glyph.bh > 0) {
            // 위치는 논리 좌표 그대로. 정수로 내리지 않는다 — 확대된 비트맵을
            // 논리 격자에 맞춰 반올림하면 배율만큼 어긋나 글자 간격이 튄다.
            const float gx = pen_x + glyph.xoff;
            const float gy = baseline + glyph.yoff;
            // channel = 1 — 셰이더가 R8 의 r 을 알파로 읽고 color 를 곱한다.
            glb_rect(s_atlas, gx, gy, glyph.w, glyph.h,
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
