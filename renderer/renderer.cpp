// renderer/renderer.cpp — CPU ARGB32 2D software renderer
//
// GPU API 없이 게임이 소유한 메모리에 직접 픽셀을 쓴다. 플랫폼 계층은
// 완성된 프레임버퍼를 운영체제 창에 복사할 뿐이며, 게임 좌표계는 언제나
// platform_init/renderer_init에 전달한 고정 논리 해상도다.

#include "renderer.h"
#include "software_internal.h"
#include "image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

void renderer_text_shutdown();

static int s_screen_w = 0;
static int s_screen_h = 0;
static int s_view_ox = 0;
static int s_view_oy = 0;
static std::vector<uint32_t> s_pixels; // 0xAARRGGBB

static uint32_t pack_opaque(Color c)
{
    return 0xFF000000u | (uint32_t(c.r) << 16) |
           (uint32_t(c.g) << 8) | uint32_t(c.b);
}

static void blend_surface(int x, int y, Color c, uint8_t coverage)
{
    if ((unsigned)x >= (unsigned)s_screen_w ||
        (unsigned)y >= (unsigned)s_screen_h) return;

    const unsigned a = (unsigned(c.a) * unsigned(coverage) + 127u) / 255u;
    if (a == 0) return;

    uint32_t& dst = s_pixels[(size_t)y * (size_t)s_screen_w + (size_t)x];
    if (a == 255) {
        dst = 0xFF000000u | (uint32_t(c.r) << 16) |
              (uint32_t(c.g) << 8) | uint32_t(c.b);
        return;
    }

    const unsigned inv = 255u - a;
    const unsigned dr = (dst >> 16) & 0xFFu;
    const unsigned dg = (dst >> 8) & 0xFFu;
    const unsigned db = dst & 0xFFu;
    const unsigned r = (unsigned(c.r) * a + dr * inv + 127u) / 255u;
    const unsigned g = (unsigned(c.g) * a + dg * inv + 127u) / 255u;
    const unsigned b = (unsigned(c.b) * a + db * inv + 127u) / 255u;
    dst = 0xFF000000u | (r << 16) | (g << 8) | b;
}

void software_blend_pixel(int x, int y, Color color)
{
    blend_surface(x + s_view_ox, y + s_view_oy, color, 255);
}

void software_blend_coverage(int x, int y, Color color, uint8_t coverage)
{
    blend_surface(x + s_view_ox, y + s_view_oy, color, coverage);
}

int software_surface_width() { return s_screen_w; }
int software_surface_height() { return s_screen_h; }

void renderer_init(int screen_w, int screen_h)
{
    s_screen_w = std::max(screen_w, 1);
    s_screen_h = std::max(screen_h, 1);
    s_view_ox = s_view_oy = 0;
    s_pixels.assign((size_t)s_screen_w * (size_t)s_screen_h, 0xFF000000u);
    image_init();
}

void renderer_begin(Color bg)
{
    std::fill(s_pixels.begin(), s_pixels.end(), pack_opaque(bg));
}

void renderer_end()
{
    if (s_pixels.empty()) return;
    platform_present(s_pixels.data(), s_screen_w, s_screen_h,
                     s_screen_w * (int)sizeof(uint32_t));
}

void renderer_set_view_offset(int dx, int dy)
{
    s_view_ox = dx;
    s_view_oy = dy;
}

void renderer_shutdown()
{
    image_shutdown();
    renderer_text_shutdown();
    s_pixels.clear();
    s_pixels.shrink_to_fit();
    s_screen_w = s_screen_h = 0;
    s_view_ox = s_view_oy = 0;
}

void draw_rect(int x, int y, int w, int h, Color c)
{
    if (w <= 0 || h <= 0 || c.a == 0) return;
    x += s_view_ox;
    y += s_view_oy;
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = std::min(x + w, s_screen_w);
    const int y1 = std::min(y + h, s_screen_h);
    if (x0 >= x1 || y0 >= y1) return;

    if (c.a == 255) {
        const uint32_t pixel = pack_opaque(c);
        for (int py = y0; py < y1; ++py) {
            uint32_t* row = s_pixels.data() + (size_t)py * (size_t)s_screen_w;
            std::fill(row + x0, row + x1, pixel);
        }
        return;
    }

    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px)
            blend_surface(px, py, c, 255);
}

void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c)
{
    if (w <= 0 || h <= 0 || c.a == 0) return;
    roundness = std::max(0.0f, std::min(roundness, 1.0f));
    const float radius = roundness * 0.5f * (float)std::min(w, h);
    if (radius < 1.0f) {
        draw_rect(x, y, w, h, c);
        return;
    }

    const float left_c = (float)x + radius;
    const float right_c = (float)(x + w) - radius;
    const float top_c = (float)y + radius;
    const float bottom_c = (float)(y + h) - radius;
    const float r2 = radius * radius;

    for (int py = y; py < y + h; ++py) {
        const float cy = (float)py + 0.5f;
        for (int px = x; px < x + w; ++px) {
            const float cx = (float)px + 0.5f;
            const float qx = std::max(left_c - cx, std::max(0.0f, cx - right_c));
            const float qy = std::max(top_c - cy, std::max(0.0f, cy - bottom_c));
            if (qx * qx + qy * qy <= r2)
                software_blend_pixel(px, py, c);
        }
    }
}
