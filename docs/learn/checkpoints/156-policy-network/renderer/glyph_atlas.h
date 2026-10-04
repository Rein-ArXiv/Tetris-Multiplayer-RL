#pragma once
#include "renderer/mask_texture.h"
#include "text/font.h"
#include <algorithm>
#include <limits>

namespace study_atlas {
// Single render thread. A process-wide monotonic stamp avoids both reset and
// object-address reuse aliases. Exhaustion fails rather than wrapping to an old ID.
inline std::uint64_t next_revision=1;
inline std::uint64_t take_revision() noexcept {
    const auto result=next_revision;
    if(result==0)return 0;
    next_revision=result==(std::numeric_limits<std::uint64_t>::max)() ? 0 : result+1;
    return result;
}
struct Region { Shelf::Rect ink{};std::uint64_t revision=0; };
class GlyphAtlas {
public:
    GlyphAtlas(const study_gl::GlApi& gl,int width,int height) : texture_(gl),shelf_(width,height) {}
    bool init() {
        if(revision_ || !next_revision || !texture_.init(shelf_.width(),shelf_.height()))return false;
        revision_=take_revision();return true;
    }
    const MaskTexture& texture() const noexcept{return texture_;}
    std::uint64_t revision() const noexcept{return revision_;}
    bool contains(const Region& region) const noexcept {
        const auto r=region.ink;
        return revision_ && region.revision==revision_ && r.x>=0 && r.y>=0 && r.w>=0 && r.h>=0 &&
               r.x<=shelf_.width() && r.y<=shelf_.height() && r.w<=shelf_.width()-r.x && r.h<=shelf_.height()-r.y;
    }
    std::optional<Region> insert(const study_font::Glyph& glyph) {
        if(!revision_ || glyph.width<0 || glyph.height<0 ||
           glyph.width>shelf_.width()-2 || glyph.height>shelf_.height()-2 ||
           glyph.coverage.size()!=std::size_t(glyph.width)*glyph.height) return std::nullopt;
        if(glyph.coverage.empty()) return Region{{},revision_};
        auto candidate=shelf_;
        const auto placement=candidate.insert(glyph.width,glyph.height);
        if(!placement)return std::nullopt;
        try {
            const auto outer=placement->outer;
            std::vector<unsigned char> padded(std::size_t(outer.w)*outer.h,0);
            for(int row=0;row<glyph.height;++row)
                std::copy_n(glyph.coverage.data()+std::size_t(row)*glyph.width,glyph.width,
                            padded.data()+std::size_t(row+1)*outer.w+1);
            if(!texture_.write(outer,padded))return std::nullopt;
            shelf_=candidate;
            return Region{placement->ink,revision_};
        } catch(const std::bad_alloc&){return std::nullopt;}
    }
    // Caller has submitted every CPU queue using old UVs. Rebuild all regions
    // after this call; even failure invalidates them because storage may change.
    bool clear() {
        if(!texture_.name() || !next_revision)return false;
        const auto revision=take_revision();revision_=0;
        if(!texture_.clear())return false;
        shelf_.clear();revision_=revision;return true;
    }
private:
    MaskTexture texture_;
    Shelf shelf_;
    std::uint64_t revision_=0;
};
} // namespace study_atlas
