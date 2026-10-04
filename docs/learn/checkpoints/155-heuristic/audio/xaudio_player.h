#pragma once

// audio/xaudio_player.h - Windows-only XAudio2 backend for the COMMON Session
// control contract (open/replace/play/stop/unload/close).
//
// One output device with a fixed pool of source voices. Clip slots own their
// PCM and several voices may borrow one clip concurrently; the no-argument
// entry points address slot zero.
//
// Threading: every control call and the destructor run on the same initiating
// main thread. Control metadata is not shared with a user callback. The worker
// reads immutable PCM; DestroyVoice provides the read-completion boundary.
// Unique graph/PCM ownership is neither copied nor moved across objects.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <xaudio2.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "audio/mix_s16.h"
#include "audio/pcm_s16.h"
#include "audio/voice_order.h"

namespace study_audio {

class XAudioPlayer {
public:
    // Number of independently startable voices and matching clip slots.
    static constexpr std::size_t kVoiceCount =
        static_cast<std::size_t>(audio_mix::kMaxVoices);
    // Sentinel owner for a voice not bound to any clip slot.
    static constexpr std::size_t kNoOwner = static_cast<std::size_t>(-1);

    XAudioPlayer() noexcept = default;
    ~XAudioPlayer();

    XAudioPlayer(const XAudioPlayer&) = delete;
    XAudioPlayer& operator=(const XAudioPlayer&) = delete;
    XAudioPlayer(XAudioPlayer&&) = delete;
    XAudioPlayer& operator=(XAudioPlayer&&) = delete;

    bool open(std::uint32_t rate, std::uint32_t channels) noexcept;
    bool replace(Pcm16&& clip, std::size_t clip_slot = 0) noexcept;
    bool play(std::size_t clip_slot = 0) noexcept;
    void stop() noexcept;
    void unload(std::size_t slot = 0) noexcept;
    void close() noexcept;

private:
    struct VoiceEntry {
        IXAudio2SourceVoice* ptr = nullptr;
        std::size_t owner = kNoOwner;
    };

    // Release one pool voice and its XAudio2 allocation.
    void destroy_slot(std::size_t index) noexcept;
    // Destroy every voice bound to owner, including already-finished ones.
    void stop_owner(std::size_t owner) noexcept;

    IXAudio2* engine_ = nullptr;
    IXAudio2MasteringVoice* master_ = nullptr;
    // True only when this object acquired a per-thread COM reference (S_OK or
    // S_FALSE from CoInitializeEx). A changed-mode caller keeps its own
    // apartment and is never uninitialized here.
    bool com_owned_ = false;
    WAVEFORMATEX format_{};
    std::array<std::optional<Pcm16>, kVoiceCount> clips_{};
    std::array<VoiceEntry, kVoiceCount> voices_{};
    audio_pool::VoiceOrder<kVoiceCount> order_{};
};

}  // namespace study_audio
