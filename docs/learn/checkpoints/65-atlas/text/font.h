#pragma once

// study_font: small CPU-only TrueType rasterizer built on stb_truetype.
//
// TRUSTED ASSETS ONLY. stb_truetype has no length-bounded parser: it trusts the
// surrounding buffer plus internal offset/length fields. Checking headers or
// file size is NOT validation. Never pass uploaded, downloaded, or otherwise
// untrusted fonts here; only prevalidated fonts packaged with the program.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace study_font {

// Vertical metrics for a requested pixel height.
struct Metrics {
    float ascent{};   // pixels above baseline
    float descent{};  // signed pixels, normally negative below baseline
    float line_gap{}; // additional leading in pixels
    float scale{};    // font units -> pixels
};

// One rasterized glyph.
struct Glyph {
    int index{};          // glyph index; 0 is .notdef
    int width{};          // coverage width in pixels
    int height{};         // coverage height in pixels
    int xoff{};           // bitmap left offset from pen origin
    int yoff{};           // bitmap top offset from baseline
    float advance{};      // horizontal advance in pixels
    float left_bearing{}; // left side bearing in pixels
    bool missing{};       // true when index == 0
    std::vector<unsigned char> coverage; // width*height, top-down 8-bit coverage
};

class Font {
public:
    Font();
    ~Font();
    Font(Font&&) noexcept;
    Font& operator=(Font&&) noexcept;
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Reads a trusted packaged TTF and initializes stb_truetype.
    // Reported failure preserves the previous font. File-size checks are
    // resource policy, not validation of the trusted asset contract.
    bool load_trusted(const std::filesystem::path& path);

    bool loaded() const noexcept;

    // pixel_height must be finite and within [1, 128].
    std::optional<Metrics> metrics(float pixel_height) const;

    // scalar must be a valid Unicode scalar value and pixel_height finite in [1, 128].
    std::optional<Glyph> rasterize(char32_t scalar, float pixel_height) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace study_font