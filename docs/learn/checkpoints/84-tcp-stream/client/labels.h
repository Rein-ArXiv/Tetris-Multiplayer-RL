#pragma once
#include "text/utf8.h"
#include <array>
#include <optional>
namespace study_labels {
// Bounded presentation data. These are Unicode scalars, not shaped glyph IDs.
struct Label {
    std::array<char32_t,16> scalars{};
    std::size_t count=0;
    std::size_t invalid_bytes=0;
};
inline std::optional<Label> decode(std::string_view input) noexcept {
    Label label;
    while(!input.empty()){
        if(label.count==label.scalars.size())return std::nullopt;
        const auto item=study_utf8::decode_first(input);
        label.scalars[label.count++]=item.codepoint;
        if(item.status==study_utf8::Status::invalid)++label.invalid_bytes;
        input.remove_prefix(item.bytes);
    }
    return label;
}
inline constexpr std::string_view menu_text=u8"대기실";
inline constexpr std::string_view play_text=u8"플레이";
struct Labels { Label menu,play; };
inline std::optional<Labels> make() noexcept {
    const auto menu=decode(menu_text),play=decode(play_text);
    if(!menu || !play)return std::nullopt;
    return Labels{*menu,*play};
}
} // namespace study_labels
