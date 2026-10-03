#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <cstddef>
#include <fstream>
#include <new>
#include <system_error>
#include <utility>

namespace study_font {
namespace {

// Deleter guarantees stbtt_FreeBitmap runs for the raw bitmap, even when a
// later allocation (e.g. the coverage vector) throws.
struct StbBitmapDeleter {
    void operator()(unsigned char* p) const noexcept {
        if (p != nullptr) stbtt_FreeBitmap(p, nullptr);
    }
};

constexpr int kMaxBitmapDim = 1024;
constexpr float kMinPixelHeight = 1.0f;
constexpr float kMaxPixelHeight = 128.0f;

bool valid_height(float pixel_height) {
    // The comparisons also reject NaN.
    return pixel_height >= kMinPixelHeight && pixel_height <= kMaxPixelHeight;
}

bool valid_scalar(char32_t scalar) {
    if (scalar > 0x10FFFFu) return false;
    if (scalar >= 0xD800u && scalar <= 0xDFFFu) return false; // surrogates
    return true;
}

} // namespace

// stb borrows bytes.data(). Moving Font transfers the Impl pointer, without
// moving or resizing this vector. Its backing allocation stays alive and
// unchanged for every use of info.
struct Font::Impl {
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool ready = false;
};

Font::Font() = default;
Font::~Font() = default;
Font::Font(Font&&) noexcept = default;
Font& Font::operator=(Font&&) noexcept = default;

bool Font::load_trusted(const std::filesystem::path& path) {
    constexpr std::uintmax_t kMaxBytes = 16u * 1024u * 1024u;

    try {
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(path, ec);
        // Reject unreadable files, empty/tiny files, and oversized files.
        if (ec || size < 12 || size > kMaxBytes) return false;

        // Build into a candidate; impl_ is only replaced after full success.
        auto candidate = std::make_unique<Impl>();
        candidate->bytes.resize(static_cast<std::size_t>(size));

        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        in.read(reinterpret_cast<char*>(candidate->bytes.data()),
                static_cast<std::streamsize>(size));
        if (!in || in.gcount() != static_cast<std::streamsize>(size)) return false;

        const unsigned char* data = candidate->bytes.data();
        const int offset = stbtt_GetFontOffsetForIndex(data, 0);
        if (offset < 0) return false;
        if (!stbtt_InitFont(&candidate->info, data, offset)) return false;

        candidate->ready = true;
        impl_ = std::move(candidate);
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool Font::loaded() const noexcept {
    return impl_ != nullptr && impl_->ready;
}

std::optional<Metrics> Font::metrics(float pixel_height) const {
    if (!loaded() || !valid_height(pixel_height)) return std::nullopt;

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&impl_->info, &ascent, &descent, &line_gap);

    const float scale = stbtt_ScaleForPixelHeight(&impl_->info, pixel_height);
    Metrics m;
    m.ascent = static_cast<float>(ascent) * scale;
    m.descent = static_cast<float>(descent) * scale;
    m.line_gap = static_cast<float>(line_gap) * scale;
    m.scale = scale;
    return m;
}

std::optional<Glyph> Font::rasterize(char32_t scalar, float pixel_height) const {
    if (!loaded() || !valid_height(pixel_height) || !valid_scalar(scalar)) {
        return std::nullopt;
    }

    try {
        const stbtt_fontinfo* info = &impl_->info;
        const float scale = stbtt_ScaleForPixelHeight(info, pixel_height);

        const int index = stbtt_FindGlyphIndex(info, static_cast<int>(scalar));
        // index 0 is .notdef. Rasterize it instead of silently skipping.
        const bool missing = (index == 0);

        int advance_units = 0, lsb_units = 0;
        stbtt_GetGlyphHMetrics(info, index, &advance_units, &lsb_units);

        Glyph glyph{};
        glyph.index = index;
        glyph.missing = missing;
        glyph.advance = static_cast<float>(advance_units) * scale;
        glyph.left_bearing = static_cast<float>(lsb_units) * scale;

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(info, index, scale, scale, &x0, &y0, &x1, &y1);

        const int width = x1 - x0;
        const int height = y1 - y0;

        // Reject negative or oversized boxes before any allocation. width and
        // height are then each <= kMaxBitmapDim, so width*height cannot overflow
        // the size_t used for the coverage vector.
        if (width < 0 || height < 0) return std::nullopt;
        if (width > kMaxBitmapDim || height > kMaxBitmapDim) return std::nullopt;

        glyph.width = width;
        glyph.height = height;
        glyph.xoff = x0;
        glyph.yoff = y0;

        // Empty shape (e.g. space): valid, keep advance, leave coverage empty.
        if (width == 0 || height == 0) return glyph;

        int bw = 0, bh = 0, bx = 0, by = 0;
        unsigned char* raw = stbtt_GetGlyphBitmap(info, scale, scale, index,
                                                  &bw, &bh, &bx, &by);
        // Own the raw bitmap immediately so the vector allocation below cannot
        // leak it on bad_alloc.
        std::unique_ptr<unsigned char, StbBitmapDeleter> bitmap(raw);

        // A positive-area box must yield a bitmap with matching dimensions.
        if (!bitmap) return std::nullopt;
        if (bw != width || bh != height || bx != x0 || by != y0) return std::nullopt;

        const std::size_t count =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        glyph.coverage.assign(bitmap.get(), bitmap.get() + count);
        return glyph;
    } catch (const std::bad_alloc&) {
        // Our own allocations are handled here, but stb_truetype's internal
        // allocations do not report OOM comprehensively, so rasterize is not
        // claimed to be fully OOM safe.
        return std::nullopt;
    }
}

} // namespace study_font