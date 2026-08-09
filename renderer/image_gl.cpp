// renderer/image_gl.cpp — 이미지 디코딩 + GPU 텍스처
//
// 디코딩은 여전히 CPU 가 한다. PNG/JPG 를 푸는 일은 GPU 가 할 수 있는 종류의
// 작업이 아니고, 어차피 로드 시점에 한 번뿐이다. 바뀐 것은 결과를 어디에
// 두느냐다 — 시스템 RAM 의 픽셀 배열 대신 GL 텍스처로 올린다.
//
// 그리기는 셋 다 사각형 하나로 끝난다.
//   draw_image        — 텍스처를 목적지 크기로 늘려 그린다 (샘플러가 확대)
//   draw_image_tinted — 같은 사각형에 색을 곱한다 (셰이더의 v_color)
//   draw_image_rotated— 네 꼭짓점을 CPU 에서 회전시켜 넘긴다
//
// CPU 구현에 있던 sample_nearest 와 역변환 루프가 전부 사라졌다. 그 일을
// 이제 텍스처 샘플러와 래스터라이저가 한다.

#include "image.h"
#include "gl_internal.h"

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
    bool   used = false;
    int    w = 0;
    int    h = 0;
    GLuint tex = 0;
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
    if (s_images.empty()) s_images.resize(1); // 핸들 0 은 무효값으로 예약
}

void image_shutdown()
{
    // 텍스처를 먼저 지운다. 컨텍스트가 살아 있을 때만 유효한 호출이라
    // renderer_shutdown 이 platform_shutdown 보다 앞서야 한다.
    for (auto& e : s_images) {
        if (e.tex) gl_DeleteTextures(1, &e.tex);
    }
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

    // 슬롯 0 은 "무효 핸들" 로 예약돼 있다. image_shutdown 이 벡터를 비운 뒤
    // 여기로 들어오면 push_back 결과가 인덱스 0 이 되어, 호출자에게는 실패로
    // 보이는데 텍스처는 이미 만들어진 상태로 새어 나간다. image_init 은
    // 멱등이므로 여기서 한 번 더 불러 그 경로를 막는다.
    image_init();

    ImageEntry entry;
    entry.used = true;
    entry.w = width;
    entry.h = height;

    gl_GenTextures(1, &entry.tex);
    gl_BindTexture(GL_TEXTURE_2D, entry.tex);
    gl_PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    // 아이콘이 픽셀아트라 확대할 때 NEAREST 로 경계를 살린다. 부드러운
    // 확대가 필요하면 이 두 줄을 GL_LINEAR 로 바꾸면 된다.
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    for (size_t i = 1; i < s_images.size(); ++i) {
        if (!s_images[i].used) {
            s_images[i] = entry;
            return (ImageHandle)i;
        }
    }
    s_images.push_back(entry);
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
    ImageEntry& e = s_images[(size_t)handle];
    if (e.tex) gl_DeleteTextures(1, &e.tex);
    e = {};
}

bool image_size(ImageHandle handle, int& width, int& height)
{
    if (handle <= 0 || (size_t)handle >= s_images.size() ||
        !s_images[(size_t)handle].used) return false;
    width = s_images[(size_t)handle].w;
    height = s_images[(size_t)handle].h;
    return true;
}

void draw_image_tinted(ImageHandle handle, int x, int y, int width, int height,
                       Color tint)
{
    if (handle <= 0 || (size_t)handle >= s_images.size()) return;
    const ImageEntry& e = s_images[(size_t)handle];
    if (!e.used || width <= 0 || height <= 0) return;

    glb_rect(e.tex, (float)x, (float)y, (float)width, (float)height,
             0.0f, 0.0f, 1.0f, 1.0f, tint, 0.0f, 0.0f);
}

void draw_image(ImageHandle handle, int x, int y, int width, int height)
{
    draw_image_tinted(handle, x, y, width, height, WHITE);
}

void draw_image_rotated(ImageHandle handle, int cx, int cy, int width, int height,
                        float clockwise_degrees)
{
    if (handle <= 0 || (size_t)handle >= s_images.size()) return;
    const ImageEntry& e = s_images[(size_t)handle];
    if (!e.used || width <= 0 || height <= 0) return;

    // 화면 좌표는 y 가 아래로 증가하므로 양의 각도가 시계 방향이 되도록
    // 부호를 맞춘다. CPU 구현이 목적지에서 원본으로 역변환했던 것과 달리,
    // 여기서는 네 꼭짓점만 정변환하면 그 사이는 래스터라이저가 채운다.
    const float rad = clockwise_degrees * 3.14159265358979323846f / 180.0f;
    const float cs = std::cos(rad);
    const float sn = std::sin(rad);
    const float hw = (float)width  * 0.5f;
    const float hh = (float)height * 0.5f;

    const float lx[4] = { -hw,  hw,  hw, -hw };
    const float ly[4] = { -hh, -hh,  hh,  hh };
    float px[4], py[4];
    for (int i = 0; i < 4; ++i) {
        px[i] = (float)cx + lx[i] * cs - ly[i] * sn;
        py[i] = (float)cy + lx[i] * sn + ly[i] * cs;
    }
    const float uu[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float vv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    glb_quad(e.tex, px, py, uu, vv, WHITE, 0.0f);
}
