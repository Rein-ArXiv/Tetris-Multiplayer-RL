#pragma once
#include "renderer/badge_pixels.h"
namespace study_texture {
inline auto make_play_badge() noexcept {
    auto pixels=badge_pixels;
    for(std::size_t i=0;i<pixels.size();i+=4) {
        if(pixels[i]==58 && pixels[i+1]==190 && pixels[i+2]==220) {
            pixels[i]=240;pixels[i+1]=170;pixels[i+2]=50;
        }
    }
    return pixels;
}
} // namespace study_texture
