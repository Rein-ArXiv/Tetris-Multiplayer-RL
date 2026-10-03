#ifndef STUDY_AUDIO_PCM_S16_H
#define STUDY_AUDIO_PCM_S16_H

// audio/pcm_s16.h - owning 16-bit PCM buffer for the teaching checkpoint.
// Data preparation only: no playback, SDL, MP3 decoding, or UI.
// Depends only on pcm_layout.h; no root UI or engine headers.

#include "audio/pcm_layout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace study_audio {

class Pcm16 {
public:
    // Teaching resource budget for prepared data (16 MiB).
    static constexpr std::size_t kTeachingBudgetBytes = 16u * 1024u * 1024u;

    // Validate and take ownership of interleaved S16 scalar samples.
    // Rejects empty input, partial trailing frames, and out-of-policy layouts.
    static std::optional<Pcm16> make(std::uint32_t rate,
                                     std::uint32_t channels,
                                     std::vector<std::int16_t> samples) {
        if (samples.empty()) return std::nullopt;
        if (channels == 0) return std::nullopt;
        if (samples.size() % channels != 0) return std::nullopt;

        const std::uint64_t frames =
            static_cast<std::uint64_t>(samples.size() / channels);
        const auto layout =
            audio_pcm::layout_s16(frames, channels, rate, kTeachingBudgetBytes);
        if (!layout) return std::nullopt;

        return Pcm16(rate, channels, std::move(samples));
    }

    // Counts are derived from owned storage, so a moved-from object's empty
    // storage cannot leave stale frame counts. Borrowed views must not outlive
    // this object or be used after its move/assignment/destruction.
    Pcm16(const Pcm16&) = default;
    Pcm16(Pcm16&&) noexcept = default;
    // Build the value parameter first. If copying its vector throws, this
    // object's format and storage still match. Commit with nonthrowing swaps.
    Pcm16& operator=(Pcm16 other) noexcept {
        std::swap(rate_, other.rate_);
        std::swap(channels_, other.channels_);
        samples_.swap(other.samples_);
        return *this;
    }

    std::uint32_t rate() const noexcept { return rate_; }
    std::uint32_t channels() const noexcept { return channels_; }
    const std::vector<std::int16_t>& samples() const noexcept { return samples_; }
    audio_pcm::Layout layout() const noexcept {
        return {samples_.size() / channels_, samples_.size(),
                samples_.size() * sizeof(std::int16_t),
                channels_ * sizeof(std::int16_t),
                static_cast<double>(samples_.size() / channels_) / rate_};
    }

    // Checked single-scalar access. frame selects all channels at one instant.
    std::int16_t at(std::size_t frame, std::size_t channel) const {
        if (channel >= channels_ || frame >= samples_.size() / channels_) {
            throw std::out_of_range("Pcm16::at frame/channel out of range");
        }
        return samples_[frame * channels_ + channel];
    }

private:
    Pcm16(std::uint32_t rate,
          std::uint32_t channels,
          std::vector<std::int16_t> samples)
        : rate_(rate),
          channels_(channels),
          samples_(std::move(samples)) {}

    std::uint32_t rate_;
    std::uint32_t channels_;
    std::vector<std::int16_t> samples_;
};

// Deterministic 44100 Hz / 2ch / 44100-frame integer triangle reference tone.
// Framewise ramp has period 100: phase = n % 100;
// ramp = phase < 25 ? phase : (phase < 75 ? 50 - phase : phase - 100).
// The same scalar is duplicated into L and R. Allocation happens outside any
// real-time path. Allocation failure propagates; an invalid internal layout
// raises logic_error.
inline Pcm16 make_reference_tone() {
    constexpr std::uint32_t kRate = 44100;
    constexpr std::uint32_t kChannels = 2;
    constexpr std::size_t kFrames = 44100;
    constexpr std::size_t kPeriod = 100;

    std::vector<std::int16_t> samples;
    samples.reserve(kFrames * kChannels);

    for (std::size_t n = 0; n < kFrames; ++n) {
        const std::size_t phase = n % kPeriod;
        int ramp;
        if (phase < 25) {
            ramp = static_cast<int>(phase);
        } else if (phase < 75) {
            ramp = 50 - static_cast<int>(phase);
        } else {
            ramp = static_cast<int>(phase) - 100;
        }
        const auto sample = static_cast<std::int16_t>(ramp * 256);
        samples.push_back(sample);  // left
        samples.push_back(sample);  // right
    }

    auto result = Pcm16::make(kRate, kChannels, std::move(samples));
    if (!result) {
        throw std::logic_error("make_reference_tone: internal layout rejected");
    }
    return std::move(*result);
}

} // namespace study_audio

#endif // STUDY_AUDIO_PCM_S16_H
