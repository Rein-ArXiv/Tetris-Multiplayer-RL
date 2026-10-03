#include "audio/player.h"

#include <utility>

namespace study_audio {
namespace {

// RAII wrapper for SDL_LockAudioDevice / SDL_UnlockAudioDevice.
class DeviceLock {
public:
    explicit DeviceLock(SDL_AudioDeviceID dev) noexcept : dev_(dev) {
        if (dev_ != 0) SDL_LockAudioDevice(dev_);
    }
    ~DeviceLock() noexcept {
        if (dev_ != 0) SDL_UnlockAudioDevice(dev_);
    }
    DeviceLock(const DeviceLock&) = delete;
    DeviceLock& operator=(const DeviceLock&) = delete;

private:
    SDL_AudioDeviceID dev_;
};

}  // namespace

Player::~Player() { close(); }

bool Player::open(std::uint32_t rate, std::uint32_t channels) noexcept {
    if (dev_ != 0) return false;
    if (rate == 0 || channels == 0) return false;
    // The PCM layer is the authority on what a valid S16 format is.
    if (!audio_pcm::layout_s16(1, channels, rate).has_value()) return false;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = static_cast<int>(rate);
    want.format = AUDIO_S16SYS;
    want.channels = static_cast<Uint8>(channels);
    want.samples = 512;
    want.callback = &Player::callback;
    want.userdata = this;

    SDL_AudioSpec have;
    SDL_zero(have);
    const SDL_AudioDeviceID dev =
        SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    if (have.freq != want.freq || have.format != want.format ||
        have.channels != want.channels) {
        SDL_CloseAudioDevice(dev);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    dev_ = dev;
    rate_ = rate;
    channels_ = channels;
    owns_audio_ = true;
    // Opened paused; userdata is fully set before any unpause.
    return true;
}

bool Player::replace(Pcm16&& clip) noexcept {
    // Validate before touching the old clip so a failed call preserves it.
    if (dev_ == 0) return false;
    if (clip.layout().frames == 0) return false;
    if (clip.rate() != rate_ || clip.channels() != channels_) return false;
    if (!audio_pcm::layout_s16(1, clip.channels(), clip.rate()).has_value())
        return false;

    DeviceLock lock(dev_);
    voice_.stop();
    clip_.emplace(std::move(clip));
    return true;
}

bool Player::play() noexcept {
    if (dev_ == 0 || !clip_.has_value()) return false;
    {
        DeviceLock lock(dev_);
        voice_.start(*clip_);
    }
    SDL_PauseAudioDevice(dev_, 0);
    return true;
}

void Player::stop() noexcept {
    if (dev_ == 0) return;
    SDL_PauseAudioDevice(dev_, 1);
    DeviceLock lock(dev_);
    voice_.stop();
}

void Player::unload() noexcept {
    if (dev_ == 0) return;
    SDL_PauseAudioDevice(dev_, 1);
    DeviceLock lock(dev_);
    voice_.stop();
    clip_.reset();
}

void Player::close() noexcept {
    if (dev_ == 0) return;
    // Stop the callback first: it borrows both `this` and `clip_`.
    SDL_CloseAudioDevice(dev_);
    voice_.stop();
    clip_.reset();
    dev_ = 0;
    if (owns_audio_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        owns_audio_ = false;
    }
}

Player::State Player::state() const noexcept {
    if (dev_ == 0) return State::closed;
    DeviceLock lock(dev_);
    if (!clip_.has_value()) return State::empty;
    if (voice_.playing()) return State::playing;
    if (voice_.cursor_frames() >= clip_->layout().frames) return State::finished;
    return State::ready;
}

std::size_t Player::cursor_frames() const noexcept {
    if (dev_ == 0) return 0;
    DeviceLock lock(dev_);
    return voice_.cursor_frames();
}

void Player::callback(void* userdata, Uint8* out, int len) noexcept {
    if (len <= 0) return;
    auto* self = static_cast<Player*>(userdata);
    self->voice_.render(out, static_cast<std::size_t>(len));
}

}  // namespace study_audio
