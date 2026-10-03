#pragma once
#include "text/font.h"
#include "client/labels.h"
#include <array>
#include <cmath>
#include <optional>
#include <vector>
#include <utility>

namespace study_font {
struct PlacedGlyph {
    Glyph glyph{};
    float pen_x = 0;
};
struct Line {
    std::array<PlacedGlyph,16> items{};
    std::size_t count = 0;
    float advance = 0;
};
// Fixed, single-line labels. This is scalar-to-glyph mapping plus advance only;
// it does not shape complex scripts, apply kerning, or switch fallback fonts.
inline std::optional<Line> prepare_line(const Font& font,
                                       const study_labels::Label& label, float height) {
    if(label.count>16 || !font.metrics(height)) return std::nullopt;
    Line line;
    for(std::size_t i=0;i<label.count;++i) {
        if(label.scalars[i]==U'\n' || label.scalars[i]==U'\r' || label.scalars[i]==U'\t')
            return std::nullopt;
        auto glyph=font.rasterize(label.scalars[i],height);
        if(!glyph) return std::nullopt;
        line.items[i]={std::move(*glyph),line.advance};
        line.advance+=line.items[i].glyph.advance;
        ++line.count;
    }
    return line;
}
// Straight white RGBA: coverage belongs in alpha, not RGB as well.
// Caller supplies a Glyph produced by Font. Empty shapes return no pixels.
inline std::vector<unsigned char> white_rgba(const Glyph& glyph) {
    std::vector<unsigned char> pixels(glyph.coverage.size()*4,255);
    for(std::size_t i=0;i<glyph.coverage.size();++i) pixels[4*i+3]=glyph.coverage[i];
    return pixels;
}
} // namespace study_font
