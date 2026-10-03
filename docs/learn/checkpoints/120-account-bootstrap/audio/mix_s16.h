#pragma once

// audio/mix_s16.h - shared scalar mixing policy for the game and teaching mixer.
// Fixed, allocation-free helpers used by study_audio::Mixer. Pure math only:
// no OS, locks, heap, or device access.
//
// Policy: every voice applies its own gain and truncates toward zero per
// scalar BEFORE the integer sum. Because each voice contributes a fixed
// integer amount for the block, the integer total does not depend on the
// order the voices are summed in. The final clamp to int16 happens exactly
// once, after all voices. This is not a claim that floating-point summation
// is generally associative; it is the per-voice integer truncation plus
// integer accumulation that makes the fixed contributions order-independent.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace audio_mix {

// At most 9 borrowed voices mix into one block. The int32 accumulator bound
// holds even if all 9 voices contribute the most negative sample at full
// gain: 9 * -32768 still fits in int32.
constexpr std::size_t kMaxVoices = 9;
constexpr std::size_t kBlockFrames = 256;

static_assert(kMaxVoices * 32768ll <=
                  std::numeric_limits<std::int32_t>::max(),
              "9 voices must fit in an int32 accumulator");

// NaN and +/-Infinity become 0. Finite gains clamp into [0, 1].
inline float normalize_gain(float gain) noexcept {
    if (!std::isfinite(gain)) return 0.0f;
    if (gain <= 0.0f) return 0.0f;
    if (gain >= 1.0f) return 1.0f;
    return gain;
}

// Normalize the gain, multiply in float, then truncate toward zero. For any
// int16 sample and normalized gain the product is within [-32768, 32767].
inline std::int32_t scaled_sample(std::int16_t sample, float gain) noexcept {
    const float product = static_cast<float>(sample) * normalize_gain(gain);
    return static_cast<std::int32_t>(product);  // truncates toward zero
}

// Clamp the fully accumulated integer sum once, after all voices.
inline std::int16_t finish_sample(std::int32_t sum) noexcept {
    if (sum < -32768) return static_cast<std::int16_t>(-32768);
    if (sum > 32767) return static_cast<std::int16_t>(32767);
    return static_cast<std::int16_t>(sum);
}

}  // namespace audio_mix
