#pragma once
#include "renderer/glyph_cache.h"
#include "renderer/image_quad.h"
#include "text/cached_paragraph.h"

namespace study_text {
// A prepared text borrows one atlas revision, not the cache. The atlas and GL
// context must outlive it. Atlas clear invalidates all text of that revision.
class CachedText {
public:
    bool init(GpuGlyphCache& cache, const study_font::CachedParagraph& line) {
        if (ready_ || !cache.atlas().revision() || line.count > items_.size()) return false;
        std::array<Item,16> candidate{};
        for (std::size_t i=0; i<line.count; ++i) {
            const auto& source=line.items[i];
            if (!source.glyph) return false;
            const auto region=cache.get(source.glyph);
            if (!region) return false;
            const auto& glyph=source.glyph->bitmap;
            const float ratio=source.glyph->logical_per_device;
            candidate[i]={*region,source.pen_x+glyph.xoff*ratio,
                          source.baseline+glyph.yoff*ratio,ratio};
        }
        // Publish the whole text after all entries succeed. Successful uploads
        // from a failed preparation remain reusable until an explicit clear.
        items_=candidate;
        count_=line.count;
        atlas_=&cache.atlas();
        revision_=atlas_->revision();
        ready_=true;
        return true;
    }
    bool draw(study_image_quad::ImageQuad& quad, float x, float baseline) const noexcept {
        if (!ready_ || atlas_->revision()!=revision_ || !std::isfinite(x) || !std::isfinite(baseline)) return false;
        for (std::size_t i=0; i<count_; ++i) if (!atlas_->contains(items_[i].region)) return false;
        const float width=float(atlas_->texture().width()),height=float(atlas_->texture().height());
        for (std::size_t i=0; i<count_; ++i) {
            const auto& item=items_[i];
            const auto r=item.region.ink;
            if (!r.w || !r.h) continue;
            study_image_quad::Draw request;
            request.rect={x+item.x,baseline+item.y,r.w*item.ratio,r.h*item.ratio};
            request.uv={r.x/width,r.y/height,(r.x+r.w)/width,(r.y+r.h)/height};
            if (!quad.draw_texture(atlas_->texture().name(),request)) return false;
        }
        return true;
    }
    void reset() noexcept {ready_=false;atlas_=nullptr;count_=0;revision_=0;}
private:
    struct Item {study_atlas::Region region{};float x=0,y=0,ratio=1;};
    std::array<Item,16> items_{};
    const study_atlas::GlyphAtlas* atlas_=nullptr;
    std::uint64_t revision_=0;
    std::size_t count_=0;
    bool ready_=false;
};
} // namespace study_text
