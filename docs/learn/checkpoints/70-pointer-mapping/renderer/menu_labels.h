#pragma once
#include "renderer/cached_text.h"
#include <array>

namespace study_menu_render {
enum class Label : std::size_t { start, play, decorations, original, alternate, previous, next, count };
// Owns paragraph snapshots and prepared text; borrows one atlas revision. Reset
// before the shared page is cleared. Preparing all entries is one publication.
class Labels {
public:
    bool init(study_font::GlyphCache& font, study_text::GpuGlyphCache& gpu, double density) {
        if (ready_) return false;
        constexpr const char* strings[]={u8"시작",u8"플레이",u8"장식 표시",u8"기본",u8"대체","<",">"};
        std::array<Entry,count> candidate{};
        for (std::size_t i=0; i<count; ++i) {
            const auto label=study_labels::decode(strings[i]);
            if (!label) return false;
            auto paragraph=study_font::prepare_cached_paragraph(font,*label,16,density);
            if (!paragraph || !candidate[i].text.init(gpu,*paragraph)) return false;
            candidate[i].paragraph=std::move(*paragraph);
        }
        entries_=std::move(candidate);
        ready_=true;
        return true;
    }
    bool centered(Label label, study_image_quad::ImageQuad& quad,
                  float center_x,float baseline) const noexcept {
        const auto index=static_cast<std::size_t>(label);
        if (!ready_ || index>=count) return false;
        const auto& entry=entries_[index];
        return entry.text.draw(quad,center_x-entry.paragraph.layout.width/2,baseline);
    }
    void reset() {entries_={};ready_=false;}
private:
    static constexpr auto count=static_cast<std::size_t>(Label::count);
    struct Entry {study_font::CachedParagraph paragraph;study_text::CachedText text;};
    std::array<Entry,count> entries_{};
    bool ready_=false;
};
} // namespace study_menu_render
