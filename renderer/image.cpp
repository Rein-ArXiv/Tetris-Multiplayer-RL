// renderer/image.cpp — image decode + CPU sprite sampling

#include "image.h"
#include "software_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <objidl.h>
  using std::min;
  using std::max;
  #include <gdiplus.h>
  #pragma comment(lib, "gdiplus.lib")
#else
  #define STB_IMAGE_IMPLEMENTATION
  #include "../third_party/stb_image.h"
#endif

struct ImageEntry {
    bool used = false;
    int w = 0;
    int h = 0;
    std::vector<uint32_t> pixels; // 0xAARRGGBB, straight alpha
};

static std::vector<ImageEntry> s_images;

#if defined(_WIN32)
static ULONG_PTR s_gdiplus_token = 0;
static bool s_gdiplus_initialized = false;
#endif

static bool decode_image(const char* path, std::vector<uint8_t>& rgba,
                         int& width, int& height)
{
#if defined(_WIN32)
    if (!s_gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&s_gdiplus_token, &input, nullptr) !=
            Gdiplus::Ok) {
            std::fprintf(stderr, "[image] GDI+ startup failed\n");
            return false;
        }
        s_gdiplus_initialized = true;
    }
    const int wide_count = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wide_count <= 0) return false;
    std::wstring wide((size_t)wide_count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide.data(), wide_count);

    Gdiplus::Bitmap bitmap(wide.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        std::fprintf(stderr, "[image] load failed: %s\n", path);
        return false;
    }
    width = (int)bitmap.GetWidth();
    height = (int)bitmap.GetHeight();
    if (width <= 0 || height <= 0) return false;

    Gdiplus::BitmapData data{};
    Gdiplus::Rect rect(0, 0, width, height);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead,
                        PixelFormat32bppARGB, &data) != Gdiplus::Ok) {
        std::fprintf(stderr, "[image] pixel lock failed: %s\n", path);
        return false;
    }
    rgba.resize((size_t)width * (size_t)height * 4);
    const uint8_t* base = static_cast<const uint8_t*>(data.Scan0);
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = base + (ptrdiff_t)y * data.Stride;
        uint8_t* dst = rgba.data() + (size_t)y * (size_t)width * 4;
        for (int x = 0; x < width; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    bitmap.UnlockBits(&data);
    return true;
#else
    int channels = 0;
    unsigned char* decoded = stbi_load(path, &width, &height, &channels, 4);
    if (!decoded) {
        std::fprintf(stderr, "[image] load failed: %s (%s)\n",
                     path, stbi_failure_reason());
        return false;
    }
    if (width <= 0 || height <= 0) {
        stbi_image_free(decoded);
        return false;
    }
    rgba.assign(decoded, decoded + (size_t)width * (size_t)height * 4);
    stbi_image_free(decoded);
    return true;
#endif
}

void image_init()
{
    if (s_images.empty()) s_images.resize(1); // handle 0 is invalid
}

void image_shutdown()
{
    s_images.clear();
#if defined(_WIN32)
    if (s_gdiplus_initialized) {
        Gdiplus::GdiplusShutdown(s_gdiplus_token);
        s_gdiplus_initialized = false;
        s_gdiplus_token = 0;
    }
#endif
}

ImageHandle image_create_rgba(const uint8_t* rgba, int width, int height)
{
    if (!rgba || width <= 0 || height <= 0) return 0;
    ImageEntry entry;
    entry.used = true;
    entry.w = width;
    entry.h = height;
    entry.pixels.resize((size_t)width * (size_t)height);
    for (size_t i = 0; i < entry.pixels.size(); ++i) {
        const uint8_t* p = rgba + i * 4;
        entry.pixels[i] = (uint32_t(p[3]) << 24) | (uint32_t(p[0]) << 16) |
                          (uint32_t(p[1]) << 8) | uint32_t(p[2]);
    }
    for (size_t i = 1; i < s_images.size(); ++i) {
        if (!s_images[i].used) {
            s_images[i] = std::move(entry);
            return (ImageHandle)i;
        }
    }
    s_images.push_back(std::move(entry));
    return (ImageHandle)(s_images.size() - 1);
}

ImageHandle image_load(const char* path)
{
    if (!path || !*path) return 0;
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
    if (!decode_image(path, rgba, width, height)) return 0;
    return image_create_rgba(rgba.data(), width, height);
}

void image_unload(ImageHandle handle)
{
    if (handle <= 0 || (size_t)handle >= s_images.size()) return;
    s_images[(size_t)handle] = {};
}

bool image_size(ImageHandle handle, int& width, int& height)
{
    if (handle <= 0 || (size_t)handle >= s_images.size() ||
        !s_images[(size_t)handle].used) return false;
    width = s_images[(size_t)handle].w;
    height = s_images[(size_t)handle].h;
    return true;
}

static Color sample_nearest(const ImageEntry& image, float u, float v, Color tint)
{
    int sx = (int)(u * image.w);
    int sy = (int)(v * image.h);
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    if (sx >= image.w) sx = image.w - 1;
    if (sy >= image.h) sy = image.h - 1;
    const uint32_t pixel =
        image.pixels[(size_t)sy * (size_t)image.w + (size_t)sx];
    Color color{
        (uint8_t)(((pixel >> 16) & 0xFFu) * tint.r / 255u),
        (uint8_t)(((pixel >> 8) & 0xFFu) * tint.g / 255u),
        (uint8_t)((pixel & 0xFFu) * tint.b / 255u),
        (uint8_t)(((pixel >> 24) & 0xFFu) * tint.a / 255u)
    };
    return color;
}

static const ImageEntry* get_image(ImageHandle handle)
{
    if (handle <= 0 || (size_t)handle >= s_images.size()) return nullptr;
    const ImageEntry& image = s_images[(size_t)handle];
    return image.used ? &image : nullptr;
}

void draw_image_tinted(ImageHandle handle, int x, int y, int width, int height,
                       Color tint)
{
    const ImageEntry* image = get_image(handle);
    if (!image || width <= 0 || height <= 0 || tint.a == 0) return;
    for (int dy = 0; dy < height; ++dy) {
        const float v = ((float)dy + 0.5f) / (float)height;
        for (int dx = 0; dx < width; ++dx) {
            const float u = ((float)dx + 0.5f) / (float)width;
            const Color color = sample_nearest(*image, u, v, tint);
            if (color.a) software_blend_pixel(x + dx, y + dy, color);
        }
    }
}

void draw_image(ImageHandle handle, int x, int y, int width, int height)
{
    draw_image_tinted(handle, x, y, width, height, WHITE);
}

void draw_image_rotated(ImageHandle handle, int cx, int cy, int width, int height,
                        float angle_deg)
{
    const ImageEntry* image = get_image(handle);
    if (!image || width <= 0 || height <= 0) return;
    const float radians = angle_deg * 3.14159265358979323846f / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float half_w = (float)width * 0.5f;
    const float half_h = (float)height * 0.5f;
    const int extent_x = (int)std::ceil(std::abs(half_w * cosine) +
                                       std::abs(half_h * sine));
    const int extent_y = (int)std::ceil(std::abs(half_w * sine) +
                                       std::abs(half_h * cosine));

    // 목적지 픽셀 중심을 -angle로 역회전해 원본 UV를 얻는다.
    for (int y = cy - extent_y; y < cy + extent_y; ++y) {
        for (int x = cx - extent_x; x < cx + extent_x; ++x) {
            const float dx = ((float)x + 0.5f) - (float)cx;
            const float dy = ((float)y + 0.5f) - (float)cy;
            const float local_x = dx * cosine + dy * sine;
            const float local_y = -dx * sine + dy * cosine;
            const float u = (local_x + half_w) / (float)width;
            const float v = (local_y + half_h) / (float)height;
            if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) continue;
            const Color color = sample_nearest(*image, u, v, WHITE);
            if (color.a) software_blend_pixel(x, y, color);
        }
    }
}
