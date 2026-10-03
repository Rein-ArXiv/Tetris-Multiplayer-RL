#pragma once
#include "text/line.h"
#include "renderer/image_store.h"

namespace study_text {
// ImageStore and its GL context must outlive TextLine. Upload once, draw often.
class TextLine {
public:
    explicit TextLine(study_image::ImageStore& images) noexcept : images_(images) {}
    ~TextLine() { reset(); }
    TextLine(const TextLine&)=delete;
    TextLine& operator=(const TextLine&)=delete;
    bool init(const study_font::Line& line) {
        if(ready_ || line.count>items_.size()) return false;
        // Reset also releases earlier uploads if a later allocation throws.
        try {
            for(std::size_t i=0;i<line.count;++i) {
                const auto& source=line.items[i];const auto& glyph=source.glyph;
                if(glyph.width<=0 || glyph.height<=0) continue; // space still advanced the CPU pen
                const auto pixels=study_font::white_rgba(glyph);
                const auto handle=images_.create({pixels.data(),pixels.size(),glyph.width,glyph.height});
                if(!handle) {reset();return false;}
                items_[count_++]={handle,source.pen_x+glyph.xoff,float(glyph.yoff),
                                 float(glyph.width),float(glyph.height)};
            }
            ready_=true;
            return true;
        } catch(...) {reset();throw;}
    }
    bool draw(study_image_quad::ImageQuad& quad,float x,float baseline) const noexcept {
        if(!ready_ || !std::isfinite(x) || !std::isfinite(baseline)) return false;
        for(std::size_t i=0;i<count_;++i) {
            const auto& item=items_[i];study_image_quad::Draw request;
            request.rect={x+item.x,baseline+item.y,item.w,item.h};
            if(!images_.draw(quad,item.handle,request)) return false;
        }
        return true;
    }
    void reset() noexcept {
        for(std::size_t i=0;i<count_;++i) images_.unload(items_[i].handle);
        count_=0;ready_=false;
    }
private:
    struct Item {study_image::Handle handle;float x,y,w,h;};
    study_image::ImageStore& images_;
    std::array<Item,16> items_{};
    std::size_t count_=0;
    bool ready_=false;
};
} // namespace study_text
