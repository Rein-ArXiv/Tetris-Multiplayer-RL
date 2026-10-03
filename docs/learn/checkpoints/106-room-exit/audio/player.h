#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "SDL.h"
#include "audio/pcm_s16.h"
#include "audio/voice_pool.h"

namespace study_audio {

// One output device with a voice pool. Clip slots own PCM, and many voices
// may borrow one clip concurrently;
// the no-argument entry points address slot zero for backward compatibility.
// Non-copyable and non-movable because the SDL callback carries `this` as
// userdata. Main/control calls are single threaded with respect to each other;
// the device lock protects the callback from those calls.
class Player {
public:
    enum class State { closed, empty, ready, playing, finished };

    Player() noexcept = default;
    ~Player();

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    bool open(std::uint32_t rate, std::uint32_t channels) noexcept;
    bool replace(Pcm16&& clip, std::size_t slot = 0) noexcept;
    // Each call starts a separate voice, stealing the oldest only if full.
    bool play(std::size_t slot = 0) noexcept;
    void stop() noexcept;
    void unload(std::size_t slot = 0) noexcept;
    void close() noexcept;

    State state(std::size_t slot = 0) const noexcept;
    // Most recent remaining voice for this clip; zero if all were detached/stolen.
    std::size_t cursor_frames(std::size_t slot = 0) const noexcept;

private:
    static void callback(void* userdata, Uint8* out, int len) noexcept;

    SDL_AudioDeviceID dev_ = 0;
    bool owns_audio_ = false;
    std::uint32_t rate_ = 0;
    std::uint32_t channels_ = 0;
    std::array<std::optional<Pcm16>, audio_mix::kMaxVoices> clips_{};
    VoicePool pool_;
};

}  // namespace study_audio
