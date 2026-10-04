#pragma once
#include "audio/mp3_decode.h"
#include "audio/pcm_s16.h"
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

namespace study_audio {
struct ClipResult {
    audio_mp3::Error error = audio_mp3::Error::none;
    std::optional<Pcm16> pcm;
    explicit operator bool() const noexcept { return pcm.has_value(); }
};
// The decoder fills owned PCM within the same budget accepted by Pcm16.
inline ClipResult load_clip(const char* path) noexcept {
    auto result = audio_mp3::load(path, {16u*1024u*1024u, Pcm16::kTeachingBudgetBytes});
    if (!result) return {result.error, std::nullopt};
    try {
        auto pcm = Pcm16::make(result.rate, result.channels, std::move(result.samples));
        if (!pcm) return {audio_mp3::Error::unsupported_format, std::nullopt};
        return {audio_mp3::Error::none, std::move(pcm)};
    } catch (const std::bad_alloc&) {
        return {audio_mp3::Error::allocation_failed, std::nullopt};
    } catch (const std::length_error&) {
        return {audio_mp3::Error::allocation_failed, std::nullopt};
    }
}
} // namespace study_audio
