#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "audio/pcm_s16.h"

namespace study_audio {

// Single-voice, pure-CPU clip reader. Borrows a const Pcm16 for the whole
// time it is bound. The source must remain alive and unchanged until stop()
// or Voice destruction. Do not move/assign the source while bound. No heap,
// mutex, OS handle, or decoder is used here. render() is a plain copy and
// is not thread-safe by itself: the caller must serialize start/stop/render.
class Voice {
public:
    Voice() noexcept = default;

    // Bind to clip and rewind. Active only for a non-empty clip.
    void start(const Pcm16& clip) noexcept {
        clip_ = &clip;
        cursor_ = 0;
        active_ = clip.layout().frames > 0;
    }

    // Detach the borrowed clip and rewind.
    void stop() noexcept {
        clip_ = nullptr;
        cursor_ = 0;
        active_ = false;
    }

    bool playing() const noexcept { return active_; }
    std::size_t cursor_frames() const noexcept { return cursor_; }

    // Copy up to `bytes` of interleaved S16 frames from the cursor, zeroing
    // the whole output first. A trailing partial frame stays zeroed. The
    // caller promises out points at bytes writable bytes (may be null
    // when bytes == 0).
    void render(void* out, std::size_t bytes) noexcept {
        auto* dst = static_cast<std::uint8_t*>(out);
        if (bytes == 0) return;
        std::memset(dst, 0, bytes);
        if (!active_ || clip_ == nullptr) return;

        const std::size_t channels = clip_->channels();
        const std::size_t bytes_per_frame = channels * sizeof(std::int16_t);
        if (bytes_per_frame == 0) return;

        const std::size_t frames = clip_->layout().frames;
        const std::size_t wanted = bytes / bytes_per_frame;
        const std::size_t available = cursor_ < frames ? frames - cursor_ : 0;
        const std::size_t copy_frames = wanted < available ? wanted : available;

        if (copy_frames > 0) {
            std::memcpy(dst,
                        clip_->samples().data() + cursor_ * channels,
                        copy_frames * bytes_per_frame);
            cursor_ += copy_frames;
        }
        if (cursor_ >= frames) active_ = false;
    }

private:
    const Pcm16* clip_ = nullptr;
    std::size_t cursor_ = 0;  // frame index into clip_
    bool active_ = false;
};

}  // namespace study_audio
