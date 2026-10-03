#pragma once

// audio/cue_pcm.h - deterministic preparation of the four presentation cues.
// Data preparation only: turns one study_sound::Kind into an owning Pcm16.
// No playback, SDL, rule state, or UI. The session calls this while preparing its device,
// before playback begins.

#include "audio/pcm_s16.h"
#include "presentation/sound_events.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace study_audio {

// Build the cue for a kind. Returns nullopt when the enum value is outside the
// four defined kinds or when the resulting layout is rejected. The waveform is
// a mono integer triangle at 44100 Hz duplicated into two channels, 4410 frames
// long, with a linear fade envelope so the first and last samples are exactly
// zero. Peak amplitude is 2000.
inline std::optional<Pcm16> make_cue(study_sound::Kind kind) {
    constexpr std::uint32_t kRate = 44100;
    constexpr std::uint32_t kChannels = 2;
    constexpr std::size_t kFrames = 4410;
    constexpr std::size_t kEnvelope = 441;
    constexpr int kPeak = 2000;
    constexpr int kPeriods[] = {100, 200, 64, 320};

    const int index = static_cast<int>(kind);
    if (index < 0 || index >= 4) return std::nullopt;

    const int period = kPeriods[index];
    const int quarter = period / 4;

    std::vector<std::int16_t> samples;
    samples.reserve(kFrames * kChannels);

    for (std::size_t n = 0; n < kFrames; ++n) {
        const int phase =
            static_cast<int>(n % static_cast<std::size_t>(period));
        int ramp;
        if (phase < quarter) {
            ramp = phase;
        } else if (phase < 3 * quarter) {
            ramp = period / 2 - phase;
        } else {
            ramp = phase - period;
        }

        const std::size_t tail = kFrames - 1 - n;
        const int envelope =
            static_cast<int>(std::min(n, std::min(tail, kEnvelope)));
        const int sample =
            (ramp * kPeak / quarter) * envelope / static_cast<int>(kEnvelope);

        const auto value = static_cast<std::int16_t>(sample);
        samples.push_back(value);  // left
        samples.push_back(value);  // right
    }

    return Pcm16::make(kRate, kChannels, std::move(samples));
}

}  // namespace study_audio