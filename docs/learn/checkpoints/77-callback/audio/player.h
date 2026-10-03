#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include "SDL.h"
#include "audio/pcm_s16.h"
#include "audio/voice.h"

namespace study_audio {

// One output device plus one Voice. Non-copyable and non-movable because the
// SDL callback carries `this` as userdata. Main/control calls are single
// threaded with respect to each other; the device lock protects the callback
// from those calls.
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
    bool replace(Pcm16&& clip) noexcept;
    bool play() noexcept;
    void stop() noexcept;
    void unload() noexcept;
    void close() noexcept;

    State state() const noexcept;
    std::size_t cursor_frames() const noexcept;

private:
    static void callback(void* userdata, Uint8* out, int len) noexcept;

    SDL_AudioDeviceID dev_ = 0;
    bool owns_audio_ = false;
    std::uint32_t rate_ = 0;
    std::uint32_t channels_ = 0;
    std::optional<Pcm16> clip_;
    Voice voice_;
};

}  // namespace study_audio
