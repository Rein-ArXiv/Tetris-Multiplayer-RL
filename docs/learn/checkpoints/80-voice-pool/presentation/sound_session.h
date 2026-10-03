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

#ifdef STUDY_AUDIO_SDL
#include "audio/cue_pcm.h"
#include "audio/player.h"
#endif

namespace study_sound {

class Session {
public:
#ifdef STUDY_AUDIO_SDL
    static constexpr bool supported = true;
#else
    static constexpr bool supported = false;
#endif

    Session() noexcept = default;

    // The callback borrows the embedded Player address, so this owner is pinned
    // in place for its lifetime.
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    // Open the shared output device and load the four cues exactly once.
    // Repeated calls after success are a no-op returning true. Any allocation
    // or device failure closes a partially opened device and returns false;
    // nothing escapes as an exception.
    bool prepare() noexcept {
#ifdef STUDY_AUDIO_SDL
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

    // Forward one prepared cue to its matching preloaded slot. Without SDL, or
    // before a successful prepare(), this is a no-op returning false.
    bool play(Kind kind) noexcept {
#ifdef STUDY_AUDIO_SDL
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
#ifdef STUDY_AUDIO_SDL
        player_.stop();
#endif
    }

private:
    static constexpr std::uint32_t kRate = 44100;
    static constexpr std::uint32_t kChannels = 2;
    static constexpr std::size_t kCueCount = 4;

#ifdef STUDY_AUDIO_SDL
    study_audio::Player player_;
#endif
    bool ready_ = false;
};

}  // namespace study_sound