// renderer/text_software.cpp — stb_truetype CPU glyph rasterization

#include "renderer.h"
#include "software_internal.h"

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

struct Glyph {
    int w = 0;
    int h = 0;
    int xoff = 0;
    int yoff = 0;
    float advance = 0.0f;
    std::vector<uint8_t> coverage;
};

static stbtt_fontinfo s_font{};
static std::vector<uint8_t> s_ttf;
static std::unordered_map<uint64_t, Glyph> s_cache;
static bool s_font_ok = false;

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

static const Glyph& glyph_for(uint32_t cp, int px)
{
    px = px < 1 ? 1 : px;
    const uint64_t key = (uint64_t(cp) << 32) | uint32_t(px);
    auto found = s_cache.find(key);
    if (found != s_cache.end()) return found->second;

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
        glyph.coverage.assign(bitmap, bitmap + (size_t)glyph.w * (size_t)glyph.h);
    }
    if (bitmap) stbtt_FreeBitmap(bitmap, nullptr);
    return s_cache.emplace(key, std::move(glyph)).first->second;
}

void renderer_load_font(const char* path)
{
    s_font_ok = false;
    s_cache.clear();
    s_ttf.clear();
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
        const int gx = (int)std::floor(pen_x + (float)glyph.xoff);
        const int gy = (int)std::floor(baseline + (float)glyph.yoff);
        for (int row = 0; row < glyph.h; ++row) {
            for (int col = 0; col < glyph.w; ++col) {
                const uint8_t coverage =
                    glyph.coverage[(size_t)row * (size_t)glyph.w + (size_t)col];
                if (coverage)
                    software_blend_coverage(gx + col, gy + row, color, coverage);
            }
        }
        pen_x += glyph.advance;
        previous = cp;
    }
}

void renderer_text_shutdown()
{
    s_cache.clear();
    s_ttf.clear();
    s_font_ok = false;
}
