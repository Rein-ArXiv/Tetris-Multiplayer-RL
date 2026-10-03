#pragma once
#include "renderer/glyph_atlas.h"
#include "renderer/image_quad.h"
#include "text/line.h"
namespace study_text {
// Borrows one atlas revision. The atlas/context must outlive this line.
class AtlasLine {
public:
    bool init(study_atlas::GlyphAtlas& atlas,const study_font::Line& line) {
        if(ready_ || !atlas.revision() || line.count>items_.size())return false;
        std::array<Item,16> candidate{};
        for(std::size_t i=0;i<line.count;++i) {
            const auto& source=line.items[i];const auto& glyph=source.glyph;
            auto region=atlas.insert(glyph);
            if(!region)return false;
            candidate[i]={*region,source.pen_x+glyph.xoff,float(glyph.yoff)};
        }
        // Failed preparation publishes no line. Successfully inserted regions
        // remain allocated until atlas clear; this is an append-only allocator.
        items_=candidate;count_=line.count;atlas_=&atlas;revision_=atlas.revision();ready_=true;return true;
    }
    bool draw(study_image_quad::ImageQuad& quad,float x,float baseline) const noexcept {
        if(!ready_ || atlas_->revision()!=revision_ || !std::isfinite(x) || !std::isfinite(baseline))return false;
        for(std::size_t i=0;i<count_;++i)if(!atlas_->contains(items_[i].region))return false;
        const float w=float(atlas_->texture().width()),h=float(atlas_->texture().height());
        for(std::size_t i=0;i<count_;++i) {
            const auto& item=items_[i];const auto r=item.region.ink;
            if(!r.w || !r.h)continue;
            study_image_quad::Draw request;
            request.rect={x+item.x,baseline+item.y,float(r.w),float(r.h)};
            request.uv={r.x/w,r.y/h,(r.x+r.w)/w,(r.y+r.h)/h};
            if(!quad.draw_texture(atlas_->texture().name(),request))return false;
        }
        return true;
    }
    void reset() noexcept {ready_=false;atlas_=nullptr;count_=0;revision_=0;}
private:
    struct Item {study_atlas::Region region{};float x=0,y=0;};
    std::array<Item,16> items_{};
    const study_atlas::GlyphAtlas* atlas_=nullptr;
    std::uint64_t revision_=0;
    std::size_t count_=0;
    bool ready_=false;
};
} // namespace study_text
