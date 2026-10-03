#pragma once

// presentation/sound_session.h - owns the optional audio device and maps one
// drained Batch cue to a preloaded clip slot, then starts a pooled voice. The session never mutates rule
// state, never logs per event, and never retries unboundedly. Prepare once
// during run_session, then drain Batch::take() through play().

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <utility>

#include "presentation/sound_events.h"

#if defined(STUDY_AUDIO_SDL) && defined(STUDY_AUDIO_XAUDIO2)
#error "Choose exactly one audio backend"
#endif
#if defined(STUDY_AUDIO_SDL) || defined(STUDY_AUDIO_XAUDIO2)
#include "audio/cue_pcm.h"
#endif
#if defined(STUDY_AUDIO_XAUDIO2)
#include "audio/xaudio_player.h"
#elif defined(STUDY_AUDIO_SDL)
#include "audio/player.h"
#endif

namespace study_sound {

class Session {
public:
#if defined(STUDY_AUDIO_SDL) || defined(STUDY_AUDIO_XAUDIO2)
    static constexpr bool supported = true;
#else
    static constexpr bool supported = false;
#endif

    Session() noexcept = default;

    // Keep the selected backend owner on the initiating main thread.
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    // Open the shared output device and load the four cues exactly once.
    // Repeated calls after success are a no-op returning true. Any allocation
    // or device failure closes a partially opened device and returns false;
    // nothing escapes as an exception.
    bool prepare() noexcept {
#if defined(STUDY_AUDIO_SDL) || defined(STUDY_AUDIO_XAUDIO2)
        if (ready_) return true;
        try {
            if (!player_.open(kRate, kChannels)) {
                player_.close();
                return false;
            }
            for (std::size_t slot = 0; slot < kCueCount; ++slot) {
                std::optional<study_audio::Pcm16> clip =
                    study_audio::make_cue(static_cast<Kind>(slot));
                if (!clip || !player_.replace(std::move(*clip), slot)) {
                    player_.close();
                    return false;
                }
            }
            ready_ = true;
            return true;
        } catch (...) {
            player_.close();
            return false;
        }
#else
        return false;
#endif
    }

    // Forward one prepared cue to its matching preloaded slot. Without a selected backend, or
    // before a successful prepare(), this is a no-op returning false.
    bool play(Kind kind) noexcept {
#if defined(STUDY_AUDIO_SDL) || defined(STUDY_AUDIO_XAUDIO2)
        if (!ready_) return false;
        const int index = static_cast<int>(kind);
        if (index < 0 || index >= static_cast<int>(kCueCount)) return false;
        return player_.play(static_cast<std::size_t>(index));
#else
        (void)kind;
        return false;
#endif
    }

    void stop() noexcept {
#if defined(STUDY_AUDIO_SDL) || defined(STUDY_AUDIO_XAUDIO2)
        player_.stop();
#endif
    }

private:
    static constexpr std::uint32_t kRate = 44100;
    static constexpr std::uint32_t kChannels = 2;
    static constexpr std::size_t kCueCount = 4;

#if defined(STUDY_AUDIO_SDL) || defined(STUDY_AUDIO_XAUDIO2)
#if defined(STUDY_AUDIO_XAUDIO2)
    study_audio::XAudioPlayer player_;
#else
    study_audio::Player player_;
#endif
#endif
    bool ready_ = false;
};

}  // namespace study_sound