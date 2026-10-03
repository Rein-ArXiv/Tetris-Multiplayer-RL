#pragma once
#include "client/ranking_text.h"
#include "renderer/cached_text.h"
namespace study_ranking_render {
class Labels final {
public:
    bool init(const study_meta::Ranking& rows,study_font::GlyphCache& font,
              study_text::GpuGlyphCache& gpu,double density) {
        if(ready_ || !study_meta::valid_ranking(rows))return false;
        std::array<std::array<Entry,3>,study_meta::kRankingLimit> candidate{};
        for(std::size_t i=0;i<rows.size();++i) {
            const auto strings=study_ranking_ui::row_text(i,rows[i]);
            for(std::size_t j=0;j<3;++j) {
                const auto label=study_labels::decode(strings[j]);if(!label)return false;
                auto paragraph=study_font::prepare_cached_paragraph(font,*label,16,density);
                if(!paragraph || !candidate[i][j].text.init(gpu,*paragraph))return false;
                candidate[i][j].paragraph=std::move(*paragraph);
            }
        }
        entries_=std::move(candidate);count_=rows.size();ready_=true;return true;
    }
    bool draw(study_image_quad::ImageQuad& quad)const {
        if(!ready_)return false;
        for(std::size_t i=0;i<count_;++i) {
            const auto& e=entries_[i];const float y=60+44*float(i);
            if(!e[0].text.draw(quad,104,y) || !e[1].text.draw(quad,104,y+18) ||
               !e[2].text.draw(quad,104+e[1].paragraph.layout.width,y+18))return false;
        }
        return true;
    }
    void reset(){entries_={};count_=0;ready_=false;}
private:
    struct Entry {study_font::CachedParagraph paragraph;study_text::CachedText text;};
    std::array<std::array<Entry,3>,study_meta::kRankingLimit> entries_{};
    std::size_t count_=0;bool ready_=false;
};
} // namespace study_ranking_render
