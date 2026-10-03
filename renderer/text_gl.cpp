// renderer/text_gl.cpp — stb_truetype 래스터화 + GPU 글리프 아틀라스
//
// 글자 모양을 만드는 일은 여전히 CPU 가 한다. stb_truetype 가 TTF 아웃라인을
// 8비트 coverage 비트맵으로 굽는다. 이 렌더러가 선택한 CPU 래스터화 경로다.
// 바뀐 것은 그 비트맵을 어디에 두느냐다.
//
//   이전: 비트맵을 CPU 메모리에 캐시하고 픽셀마다 프레임버퍼에 합성
//   지금: 비트맵을 한 장의 R8 텍스처(아틀라스)에 올리고, 그릴 때는
//         그 텍스처의 일부를 가리키는 사각형 하나만 배처에 넣는다
//
// 아틀라스는 글리프 사이의 텍스처 교체를 줄인다. 같은 텍스처와 클립 등
// 배치 조건을 유지하는 연속 글리프는 함께 제출할 수 있다. 다른 이미지,
// 상태 변경, 배처 용량에 따라 한 문자열도 여러 제출로 나뉠 수 있다.
//
// 해상도에 대해: 사각형은 정점 좌표가 실수라 창을 4K 로 키워도 GPU 가 그
// 해상도로 다시 래스터화한다 — 저절로 선명하다. 글자는 그렇지 않다. 한 번
// 구운 비트맵을 확대하면 그 배율만큼 뭉갠다. 그래서 여기서는 배치는 논리
// 좌표로 하되, **굽는 크기만** 화면 배율을 곱해 키운다. 22px 글자를 3.4배
// 창에서 보면 배율을 3.375로 양자화해 높이 74로 구운 뒤 논리 크기로 배치한다.

#include "renderer.h"
#include "gl_internal.h"
#include "mask_upload.h"
#include "font_raster_policy.h"
#include "../core/utf8.h"

#include <algorithm>
#include <cmath>
#include <limits>
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

// Adapter for the public NUL-terminated text API. Borrow at most four bytes;
// stop at the terminator before considering another byte. The caller supplies
// a readable C string. Malformed input advances one byte, preserving later ASCII.
static uint32_t utf8_next(const char** text)
{
    std::size_t available = 0;
    while (available < 4 && (*text)[available] != '\0') ++available;
    const auto result = utf8::decode_first(std::string_view(*text, available));
    *text += result.bytes;
    return static_cast<uint32_t>(result.codepoint);
}

static bool ensure_atlas()
{
    if (s_atlas) return true;
    if (gl_GetError()) return false;
    GLint max_dim = 0;
    gl_GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_dim);
    if (gl_GetError() || max_dim < 3) return false;
    const int dimension = std::min(kAtlasWanted, int(max_dim));
    const GLuint candidate = text_detail::create_mask8(dimension);
    if (!candidate) return false;
    s_atlas = candidate;
    s_atlas_dim = dimension;
    s_pen_x = s_pen_y = s_row_h = 0;
    return true;
}

// shelf packing: 왼쪽에서 오른쪽으로 채우다 폭이 모자라면 다음 줄로 내린다.
// 반환한 UV를 옮기지 않는 단순 배치다. 입력 순서/높이에 따라 빈 공간이 남는다.
static bool pack_glyph(const uint8_t* bitmap, int w, int h, Glyph& out)
{
    if (w < 0 || h < 0) return false;
    if (w == 0 || h == 0) return true;
    if (!bitmap || w > s_atlas_dim - 2 || h > s_atlas_dim - 2) return false;
    const int outer_w = w + 2, outer_h = h + 2;
    try {
        // Own every border texel. Reused atlas space may contain older ink.
        std::vector<uint8_t> padded(std::size_t(outer_w) * outer_h, 0);
        for (int row = 0; row < h; ++row)
            std::copy_n(bitmap + std::size_t(row) * w, w,
                        padded.data() + std::size_t(row + 1) * outer_w + 1);

        int x = s_pen_x, y = s_pen_y, row_h = s_row_h;
        if (outer_w > s_atlas_dim - x) { x = 0; y += row_h; row_h = 0; }
        if (outer_h > s_atlas_dim - y) {
            glb_flush(); // Submit old UVs before any reused pixel is overwritten.
            s_cache.clear();
            s_pen_x = s_pen_y = s_row_h = 0;
            x = y = row_h = 0;
        }
        if (!text_detail::update_mask8(s_atlas, x, y, outer_w, outer_h, padded.data()))
            return false;
        out.u0 = float(x + 1) / s_atlas_dim;
        out.v0 = float(y + 1) / s_atlas_dim;
        out.u1 = float(x + 1 + w) / s_atlas_dim;
        out.v1 = float(y + 1 + h) / s_atlas_dim;
        s_pen_x = x + outer_w;
        s_pen_y = y;
        s_row_h = std::max(row_h, outer_h);
        return true;
    } catch (const std::bad_alloc&) { return false; }
}

static Glyph glyph_for(uint32_t cp, int px)
{
    px = px < 1 ? 1 : px;

    const auto plan = font_raster::plan(cp, px, glb_render_scale());
    if (plan) {
        const auto found = s_cache.find(plan->key);
        if (found != s_cache.end()) return found->second;
    }

    Glyph glyph;

    // 배치용 메트릭은 **논리 크기 기준**으로 낸다. 창을 늘렸다고 글자 간격이
    // 달라지면 버튼 안의 텍스트가 넘치는 식으로 레이아웃이 흔들린다.
    const float layout_scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
    int advance = 0;
    int left_bearing = 0;
    stbtt_GetCodepointHMetrics(&s_font, (int)cp, &advance, &left_bearing);
    glyph.advance = (float)advance * layout_scale;
    if (!plan || !ensure_atlas()) return glyph;
    const int dev_px = plan->device_height;
    const uint64_t key = plan->key;

    // 비트맵만 확대된 크기로 굽는다.
    const float bake_scale = stbtt_ScaleForPixelHeight(&s_font, (float)dev_px);
    // Reject unsupported bitmap extents before stb allocates their pixels.
    int x0=0, y0=0, x1=0, y1=0;
    stbtt_GetCodepointBitmapBox(&s_font, (int)cp, bake_scale, bake_scale, &x0, &y0, &x1, &y1);
    const int64_t width = int64_t(x1) - x0, height = int64_t(y1) - y0;
    if (width < 0 || height < 0 || width > s_atlas_dim - 2 || height > s_atlas_dim - 2)
        return glyph;
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

    // A non-empty outline can report dimensions even when bitmap allocation
    // fails. Keep spacing, suppress drawing, and allow the next request to retry.
    if (!bitmap && glyph.bw > 0 && glyph.bh > 0) {
        glyph.bw = glyph.bh = 0;
        return glyph;
    }
    if (bitmap && glyph.bw > 0 && glyph.bh > 0) {
        if (!pack_glyph(bitmap, glyph.bw, glyph.bh, glyph)) {
            glyph.bw = glyph.bh = 0;
            stbtt_FreeBitmap(bitmap, nullptr);
            return glyph; // Do not cache a failed upload; a later request may retry.
        }
    }
    if (bitmap) stbtt_FreeBitmap(bitmap, nullptr);
    return s_cache.emplace(key, glyph).first->second;
}

bool renderer_load_font(const char* path)
{
    // Submit queued quads before resetting atlas positions for another font.
    glb_flush();
    s_font_ok = false;
    s_cache.clear();
    s_ttf.clear();
    // 폰트가 바뀌면 아틀라스 내용이 의미를 잃으므로 커서를 되감는다.
    s_pen_x = s_pen_y = s_row_h = 0;
    if (!path || !*path) return false;

    FILE* file = std::fopen(path, "rb");
    if (!file) {
        std::fprintf(stderr, "[text] font open failed: %s\n", path);
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        std::fprintf(stderr, "[text] font empty: %s\n", path);
        return false;
    }
    s_ttf.resize((size_t)size);
    const size_t read = std::fread(s_ttf.data(), 1, s_ttf.size(), file);
    std::fclose(file);
    if (read != s_ttf.size()) {
        s_ttf.clear();
        std::fprintf(stderr, "[text] font read failed: %s\n", path);
        return false;
    }

    const int offset = stbtt_GetFontOffsetForIndex(s_ttf.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&s_font, s_ttf.data(), offset)) {
        s_ttf.clear();
        std::fprintf(stderr, "[text] invalid TTF: %s\n", path);
        return false;
    }
    s_font_ok = true;
    return true;
}

int measure_text(const char* text, int size)
{
    if (!text || !*text || !s_font_ok) return 0;
    const int px = size < 1 ? 1 : size;
    const float scale = stbtt_ScaleForPixelHeight(&s_font, (float)px);
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
        if (previous)
            line_width += stbtt_GetCodepointKernAdvance(
                &s_font, (int)previous, (int)cp) * scale;
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&s_font, (int)cp, &advance, &bearing);
        line_width += advance * scale;
        previous = cp;
    }
    max_width = std::max(max_width, line_width);
    // Measurement uses CPU metrics only; it must not allocate or recycle an atlas.
    const double rounded = std::floor(double(max_width) + 0.5);
    if (!(rounded < (std::numeric_limits<int>::max)()))
        return (std::numeric_limits<int>::max)();
    return int(rounded);
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
        glb_before_texture_delete(s_atlas);
        gl_DeleteTextures(1, &s_atlas);
        s_atlas = 0;
    }
    s_cache.clear();
    s_ttf.clear();
    s_pen_x = s_pen_y = s_row_h = 0;
    s_font_ok = false;
}
