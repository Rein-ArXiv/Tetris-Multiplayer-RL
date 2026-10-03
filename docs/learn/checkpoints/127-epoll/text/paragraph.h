#pragma once
#include "text/font.h"
#include "text/layout.h"
#include "client/labels.h"
#include <array>
#include <utility>

namespace study_font {
struct TextGlyph {
    Glyph glyph{};
    float pen_x = 0;
    float baseline = 0;
};
struct Paragraph {
    std::array<TextGlyph,16> items{};
    std::size_t count = 0;
    study_layout::Result layout{};
};
// Bounded scalar-by-scalar horizontal text. LF starts a line; CR/TAB are rejected.
// The owned result is both the measurement and the input to GPU preparation.
inline std::optional<Paragraph> prepare_paragraph(const Font& font,
                                                  const study_labels::Label& label,
                                                  float height) {
    if (label.count > label.scalars.size()) return std::nullopt;
    const auto metrics = font.metrics(height);
    if (!metrics) return std::nullopt;
    study_layout::Layout layout({metrics->ascent, metrics->descent, metrics->line_gap});
    if (!layout.valid()) return std::nullopt;
    Paragraph paragraph;
    int previous = 0;
    for (std::size_t i=0; i<label.count; ++i) {
        const auto scalar = label.scalars[i];
        if (scalar == U'\r' || scalar == U'\t') return std::nullopt;
        if (scalar == U'\n') {
            if (!layout.newline()) return std::nullopt;
            continue;
        }
        auto glyph = font.rasterize(scalar, height);
        if (!glyph) return std::nullopt;
        const auto kern = layout.has_previous()
            ? font.kerning(previous, glyph->index, height) : std::optional<float>(0);
        if (!kern || !layout.add(glyph->advance, *kern,
            {float(glyph->xoff),float(glyph->yoff),float(glyph->width),float(glyph->height)}))
            return std::nullopt;
        previous = glyph->index;
        paragraph.items[paragraph.count++].glyph = std::move(*glyph);
    }
    paragraph.layout = layout.result();
    for (std::size_t i=0; i<paragraph.count; ++i) {
        paragraph.items[i].pen_x = paragraph.layout.items[i].pen_x;
        paragraph.items[i].baseline = paragraph.layout.items[i].baseline;
    }
    return paragraph;
}
} // namespace study_font
