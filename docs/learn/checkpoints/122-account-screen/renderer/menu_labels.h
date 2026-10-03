#pragma once
#include "renderer/cached_text.h"
#include <array>
#include "content/characters.h"

namespace study_menu_render {
enum class Label : std::size_t { start, play, decorations, previous, next, account, connect, back, idle, busy, online, offline, unsaved, blocked, rejected, uncertain, failed, lose_key, count };
// Owns paragraph snapshots and prepared text; borrows one atlas revision. Reset
// before the shared page is cleared. Preparing all entries is one publication.
class Labels {
public:
    bool init(study_font::GlyphCache& font, study_text::GpuGlyphCache& gpu, double density) {
        if (ready_) return false;
        constexpr const char* strings[]={u8"시작",u8"플레이",u8"장식 표시","<",">",u8"계정",u8"연결 · 다시 시도",u8"뒤로",u8"계정 연결 전",u8"계정 처리 중",u8"계정 연결됨",u8"저장됨 · 서버 확인 필요",u8"키 저장 필요",u8"계정 파일 확인 필요",u8"저장된 키 거절됨",u8"발급 결과 미확인",u8"작업을 마치지 못함",u8"종료하면 키를 잃어요"};
        std::array<Entry,count> candidate{};
        for (std::size_t i=0; i<count; ++i) {
            const char* source=i<common_count ? strings[i] : study_characters::characters[i-common_count].name;
            const auto label=study_labels::decode(source);
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
        if (index>=common_count) return false;
        return centered_at(index,quad,center_x,baseline);
    }
    bool character(std::size_t index, study_image_quad::ImageQuad& quad,
                   float center_x,float baseline) const noexcept {
        if(index>=study_characters::characters.size()) return false;
        return centered_at(common_count+index,quad,center_x,baseline);
    }
    void reset() {entries_={};ready_=false;}
private:
    bool centered_at(std::size_t index,study_image_quad::ImageQuad& quad,
                     float center_x,float baseline) const noexcept {
        if (!ready_) return false;
        const auto& entry=entries_[index];
        return entry.text.draw(quad,center_x-entry.paragraph.layout.width/2,baseline);
    }
    static constexpr auto common_count=static_cast<std::size_t>(Label::count);
    static constexpr auto count=common_count+study_characters::characters.size();
    struct Entry {study_font::CachedParagraph paragraph;study_text::CachedText text;};
    std::array<Entry,count> entries_{};
    bool ready_=false;
};
} // namespace study_menu_render
