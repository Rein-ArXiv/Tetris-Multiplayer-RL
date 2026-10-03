#pragma once

// study_font: cached-paragraph preparation.
//
// Same shape as prepare_paragraph in "text/paragraph.h", but each item holds a
// shared_ptr snapshot of a GlyphCache entry instead of an owned Glyph bitmap.
// The logical height and render scale are validated even for an empty label, so
// one policy covers every call. A failed paragraph publishes nothing, while
// glyphs the cache already committed during the attempt stay cached.

#include "client/labels.h"       // study_labels::Label
#include "text/glyph_cache.h"    // study_font::GlyphCache, CachedGlyph, Metrics
#include "text/layout.h"         // study_layout::Layout, Result, Box
#include "text/raster_policy.h"  // font_raster::plan

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace study_font {

// One placed item: a shared, immutable glyph snapshot plus its layout origin.
struct CachedTextGlyph {
    std::shared_ptr<const CachedGlyph> glyph;
    float pen_x = 0.0f;
    float baseline = 0.0f;
};

// Bounded scalar-by-scalar text. items[0..count) are placed; the rest unused.
struct CachedParagraph {
    std::array<CachedTextGlyph, 16> items{};
    std::size_t count = 0;
    study_layout::Result layout{};
};

// Builds a cached paragraph from label at logical height and render scale.
// LF starts a new line; CR and TAB are rejected. Bitmap rectangles are stored
// in device pixels, so each is multiplied by CachedGlyph::logical_per_device
// before it enters the layout.
inline std::optional<CachedParagraph> prepare_cached_paragraph(
        GlyphCache& cache,
        const study_labels::Label& label,
        int height,
        double scale) {
    if (label.count > label.scalars.size()) {
        return std::nullopt;
    }

    // Validate the logical height and the render scale before anything else,
    // including for an empty label: the same domain checks apply whether or not
    // any scalar is present.
    if (!font_raster::plan(0u, height, scale)) {
        return std::nullopt;
    }
    const std::optional<Metrics> metrics = cache.metrics(static_cast<float>(height));
    if (!metrics) {
        return std::nullopt;
    }

    study_layout::Layout layout(
        {metrics->ascent, metrics->descent, metrics->line_gap});
    if (!layout.valid()) {
        return std::nullopt;
    }

    CachedParagraph paragraph;
    int previous = 0;
    for (std::size_t i = 0; i < label.count; ++i) {
        const char32_t scalar = label.scalars[i];
        if (scalar == U'\r' || scalar == U'\t') {
            return std::nullopt;
        }
        if (scalar == U'\n') {
            if (!layout.newline()) {
                return std::nullopt;
            }
            continue;
        }

        // get() returns a snapshot and may commit one cache entry. That entry is
        // not rolled back if a later scalar fails this paragraph.
        const std::shared_ptr<const CachedGlyph> glyph =
            cache.get(scalar, height, scale);
        if (!glyph) {
            return std::nullopt;
        }

        // Kerning is measured between glyph indices, in logical pixels.
        const std::optional<float> kern = layout.has_previous()
            ? cache.kerning(previous, glyph->bitmap.index,
                            static_cast<float>(height))
            : std::optional<float>(0.0f);

        // Device-pixel bitmap rectangle -> logical layout rectangle.
        const float logical_per_device = glyph->logical_per_device;
        const study_layout::Box box{
            static_cast<float>(glyph->bitmap.xoff) * logical_per_device,
            static_cast<float>(glyph->bitmap.yoff) * logical_per_device,
            static_cast<float>(glyph->bitmap.width) * logical_per_device,
            static_cast<float>(glyph->bitmap.height) * logical_per_device,
        };

        if (!kern || !layout.add(glyph->advance, *kern, box)) {
            return std::nullopt;
        }
        if (paragraph.count >= paragraph.items.size()) {
            return std::nullopt;  // layout caps at 16; this keeps the array safe
        }

        previous = glyph->bitmap.index;
        paragraph.items[paragraph.count].glyph = glyph;
        ++paragraph.count;
    }

    paragraph.layout = layout.result();
    for (std::size_t i = 0; i < paragraph.count; ++i) {
        paragraph.items[i].pen_x = paragraph.layout.items[i].pen_x;
        paragraph.items[i].baseline = paragraph.layout.items[i].baseline;
    }
    return paragraph;
}

}  // namespace study_font
