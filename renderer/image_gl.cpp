// renderer/image_gl.cpp — 이미지 디코딩 + GPU 텍스처
//
// 이 구현은 PNG/JPG 를 CPU 에서 RGBA 픽셀로 디코딩한 뒤 GL 텍스처로 올린다.
// 파일 형식 해석과 GPU 저장소 업로드를 분리하며, 렌더링에는 업로드한 자원을 쓴다.
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
#include "texture_upload.h"
#include "image_rows.h"
#include "handle_pool.h"
#include <memory>
#include <limits>
#include <new>
#include <stdexcept>

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
    int    w = 0;
    int    h = 0;
    GLuint tex = 0;
};

static image_detail::HandlePool<ImageEntry> s_images;

#if defined(_WIN32)
static ULONG_PTR s_gdiplus_token = 0;
static bool s_gdiplus_initialized = false;
#endif

#if defined(_WIN32)
// LockBits lends a temporary pixel buffer. Release it on every exit path.
struct BitmapReadLock {
    Gdiplus::Bitmap& bitmap;
    Gdiplus::BitmapData data{};
    bool active=false;
    explicit BitmapReadLock(Gdiplus::Bitmap& value) noexcept : bitmap(value) {}
    ~BitmapReadLock() { if(active) bitmap.UnlockBits(&data); }
    BitmapReadLock(const BitmapReadLock&)=delete;
    BitmapReadLock& operator=(const BitmapReadLock&)=delete;
    bool close() noexcept {
        if(!active) return true;
        active=false;
        return bitmap.UnlockBits(&data)==Gdiplus::Ok;
    }
};
#endif

static bool decode_image(const char* path, std::vector<uint8_t>& rgba,
                         int& width, int& height)
{
    if(!path || !*path) return false;
    try {
        int w=0,h=0;
        std::vector<uint8_t> result;
#if defined(_WIN32)
        if (!s_gdiplus_initialized) {
            Gdiplus::GdiplusStartupInput input;
            if (Gdiplus::GdiplusStartup(&s_gdiplus_token, &input, nullptr)!=Gdiplus::Ok)
                return false;
            s_gdiplus_initialized=true;
        }
        const int wide_count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,nullptr,0);
        if(wide_count<=0) return false;
        std::wstring wide(static_cast<size_t>(wide_count),L'\0');
        if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide.data(),wide_count)!=wide_count)
            return false;
        Gdiplus::Bitmap bitmap(wide.c_str());
        if(bitmap.GetLastStatus()!=Gdiplus::Ok) return false;
        const auto bw=bitmap.GetWidth(),bh=bitmap.GetHeight();
        const auto max_int=static_cast<UINT>((std::numeric_limits<int>::max)());
        if(bw>max_int || bh>max_int) return false;
        w=static_cast<int>(bw);h=static_cast<int>(bh);
        const auto bytes=image_detail::rgba_storage_bytes(w,h);
        if(!bytes) return false;
        result.resize(*bytes); // Allocate before acquiring the temporary lock.
        BitmapReadLock lock(bitmap);
        Gdiplus::Rect rect(0,0,w,h);
        if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&lock.data)!=Gdiplus::Ok)
            return false;
        lock.active=true;
        if(!image_detail::copy_bgra_rows(static_cast<const uint8_t*>(lock.data.Scan0),
                lock.data.Stride,w,h,result.data(),result.size())) return false;
        if(!lock.close()) return false;
#else
        int channels=0;
        using Pixels=std::unique_ptr<unsigned char,decltype(&stbi_image_free)>;
        Pixels decoded(stbi_load(path,&w,&h,&channels,4),stbi_image_free);
        if(!decoded) {
            const char* reason=stbi_failure_reason();
            std::fprintf(stderr,"[image] load failed: %s (%s)\n",path,reason?reason:"unknown");
            return false;
        }
        const auto bytes=image_detail::rgba_storage_bytes(w,h);
        if(!bytes) return false;
        result.assign(decoded.get(),decoded.get()+*bytes);
#endif
        rgba.swap(result);
        width=w;height=h;
        return true;
    } catch(const std::bad_alloc&) { return false; }
      catch(const std::length_error&) { return false; }
}

void image_init()
{
    // Slots are allocated lazily. Issued stamps survive shutdown/reinitialization.
}

void image_shutdown()
{
    // 텍스처를 먼저 지운다. 컨텍스트가 살아 있을 때만 유효한 호출이라
    // renderer_shutdown 이 platform_shutdown 보다 앞서야 한다.
    s_images.for_each([](ImageEntry& e) {
        if (e.tex) {
            glb_before_texture_delete(e.tex);
            gl_DeleteTextures(1, &e.tex);
        }
    });
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
    std::unique_ptr<ImageEntry> entry;
    try {
        entry=std::make_unique<ImageEntry>();
    } catch(const std::bad_alloc&) { return 0; }
    const GLuint tex=image_detail::upload_rgba8(rgba,width,height);
    if(!tex) return 0;
    entry->w=width;entry->h=height;entry->tex=tex;
    const ImageHandle handle=s_images.insert(std::move(entry));
    // Pool insertion consumes entry on every path. A failed registration still
    // leaves us responsible for the GL object, which has never been queued.
    if(!handle) gl_DeleteTextures(1,&tex);
    return handle;
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
    const ImageEntry* entry=s_images.find(handle);
    if(!entry) return;
    if(entry->tex) {
        glb_before_texture_delete(entry->tex);
        gl_DeleteTextures(1,&entry->tex);
    }
    s_images.erase(handle);
}

bool image_size(ImageHandle handle, int& width, int& height)
{
    const ImageEntry* entry=s_images.find(handle);
    if(!entry) return false;
    width=entry->w;height=entry->h;
    return true;
}

void draw_image_tinted(ImageHandle handle, int x, int y, int width, int height,
                       Color tint)
{
    const ImageEntry* entry=s_images.find(handle);
    if(!entry || width <= 0 || height <= 0) return;
    const ImageEntry& e=*entry;

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
    const ImageEntry* entry=s_images.find(handle);
    if(!entry || width <= 0 || height <= 0 || !std::isfinite(clockwise_degrees)) return;
    const ImageEntry& e=*entry;

    // 화면 좌표는 y 가 아래로 증가하므로 양의 각도가 시계 방향이 되도록
    // 부호를 맞춘다. CPU 구현이 목적지에서 원본으로 역변환했던 것과 달리,
    // 여기서는 네 꼭짓점만 정변환하면 그 사이는 래스터라이저가 채운다.
    // Reduce before multiplying: even a finite float angle can overflow the
    // old float degree-to-radian product. NaN/Inf are rejected before queuing.
    const double degrees=std::remainder(static_cast<double>(clockwise_degrees),360.0);
    const double rad=degrees*(3.14159265358979323846/180.0);
    const float cs=static_cast<float>(std::cos(rad));
    const float sn=static_cast<float>(std::sin(rad));
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
