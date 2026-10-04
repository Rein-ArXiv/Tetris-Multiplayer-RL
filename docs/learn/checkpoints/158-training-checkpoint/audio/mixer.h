#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "audio/mix_s16.h"
#include "audio/pcm_s16.h"
#include "audio/voice.h"

namespace study_audio {

// Fixed-capacity, pure-CPU mixer. Borrows up to audio_mix::kMaxVoices clips in
// explicitly indexed slots: the caller picks the slot and there is no
// automatic pool stealing. The default state is an invalid format, so start()
// and render() do nothing useful until configure() succeeds. No OS handle,
// lock, or heap allocation is used; the caller must serialize
// configure/start/stop/render on the main or audio-callback thread.
class Mixer {
public:
    Mixer() noexcept = default;

    // Validate a 16-bit interleaved scalar format. The layout is checked
    // before any mutation; on success every voice is stopped and the new
    // format is stored. Returns false and leaves the mixer untouched when the
    // layout is unsupported.
    bool configure(std::uint32_t rate, std::uint32_t channels) noexcept {
        const auto layout = audio_pcm::layout_s16(1, channels, rate);
        if (!layout) return false;
        stop_all();
        rate_ = rate;
        channels_ = channels;
        configured_ = true;
        return true;
    }

    // Bind slot to clip and (re)start it with a normalized gain. An
    // out-of-range slot, an unconfigured mixer, an empty clip, and a format
    // mismatch are all rejected before the existing voice is touched. The clip
    // must stay alive and unchanged until replacement, stop(), configure(), or
    // destruction.
    bool start(std::size_t slot, const Pcm16& clip, float gain = 1.0f) noexcept {
        if (slot >= kSlots) return false;
        if (!configured_) return false;
        if (clip.samples().empty()) return false;
        if (clip.rate() != rate_) return false;
        if (clip.channels() != channels_) return false;

        slots_[slot].gain = audio_mix::normalize_gain(gain);
        slots_[slot].voice.start(clip);
        return true;
    }

    // Store a normalized gain. Works on idle slots too.
    bool set_gain(std::size_t slot, float gain) noexcept {
        if (slot >= kSlots) return false;
        slots_[slot].gain = audio_mix::normalize_gain(gain);
        return true;
    }

    // Silence a slot. Bad indices are ignored.
    void stop(std::size_t slot) noexcept {
        if (slot < kSlots) slots_[slot].voice.stop();
    }

    void stop_all() noexcept {
        for (Slot& slot : slots_) slot.voice.stop();
    }

    bool playing(std::size_t slot) const noexcept {
        return slot < kSlots && slots_[slot].voice.playing();
    }

    std::size_t cursor_frames(std::size_t slot) const noexcept {
        return slot < kSlots ? slots_[slot].voice.cursor_frames() : 0;
    }

    // Mix into out, which must be writable. The whole range is zeroed first;
    // a trailing partial frame stays zeroed, and an invalid format leaves all
    // zeros. Work proceeds in blocks of at most kBlockFrames frames and
    // accumulates int32 sums over fixed 3 KiB stack scratch/accumulator arrays
    // (no heap).
    void render(void* out, std::size_t bytes) noexcept {
        if (bytes == 0) return;

        auto* dst = static_cast<std::uint8_t*>(out);
        std::memset(dst, 0, bytes);
        if (!configured_ || channels_ == 0) return;

        const std::size_t frame_bytes = channels_ * sizeof(std::int16_t);

        const std::size_t total_frames = bytes / frame_bytes;
        if (total_frames == 0) return;  // partial frame only: stays zeroed

        std::array<std::int16_t, kScratchSamples> scratch{};
        std::array<std::int32_t, kScratchSamples> sum{};

        std::size_t offset = 0;
        while (offset < total_frames) {
            std::size_t block = total_frames - offset;
            if (block > audio_mix::kBlockFrames) block = audio_mix::kBlockFrames;

            const std::size_t scalars = block * channels_;
            sum.fill(0);

            for (std::size_t s = 0; s < kSlots; ++s) {
                if (!slots_[s].voice.playing()) continue;
                // Render even when the gain is 0 so the cursor still advances.
                slots_[s].voice.render(scratch.data(),
                                       scalars * sizeof(std::int16_t));
                const float gain = slots_[s].gain;
                for (std::size_t i = 0; i < scalars; ++i) {
                    sum[i] += audio_mix::scaled_sample(scratch[i], gain);
                }
            }

            // memcpy each scalar so an unaligned byte destination is never
            // reinterpreted as int16_t.
            std::uint8_t* block_out = dst + offset * frame_bytes;
            for (std::size_t i = 0; i < scalars; ++i) {
                const std::int16_t value = audio_mix::finish_sample(sum[i]);
                std::memcpy(block_out + i * sizeof(std::int16_t), &value,
                            sizeof(value));
            }
            offset += block;
        }
    }

private:
    static constexpr std::size_t kSlots = audio_mix::kMaxVoices;
    static constexpr std::size_t kScratchSamples = audio_mix::kBlockFrames * 2;

    struct Slot {
        Voice voice;
        float gain = 1.0f;
    };

    std::array<Slot, kSlots> slots_{};
    std::uint32_t rate_ = 0;      // invalid until configure succeeds
    std::uint32_t channels_ = 0;  // invalid until configure succeeds
    bool configured_ = false;
};

}  // namespace study_audio
