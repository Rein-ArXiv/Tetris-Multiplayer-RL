#ifndef AUDIO_PCM_LAYOUT_H
#define AUDIO_PCM_LAYOUT_H

// pcm_layout.h - pure, allocation-free PCM S16 layout math.
// Shared by the game root and the audio data layer. No UI or engine deps.

#include <climits>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace audio_pcm {

// 16-bit signed PCM is assumed byte-exact and exactly two bytes per sample.
static_assert(CHAR_BIT == 8, "audio_pcm requires 8-bit bytes");
static_assert(sizeof(int16_t) == 2, "audio_pcm requires 16-bit int16_t");

struct Layout {
    std::size_t frames = 0;           // one instant across all channels
    std::size_t samples = 0;          // scalar samples (frames * channels)
    std::size_t bytes = 0;            // total payload bytes
    std::size_t bytes_per_frame = 0;  // channels * sizeof(int16_t)
    double seconds = 0.0;             // frames / rate
};

// Application policy: mono/stereo S16 PCM, not a universal PCM format limit.
inline constexpr std::uint32_t kMinRate = 8000;
inline constexpr std::uint32_t kMaxRate = 192000;
inline constexpr std::uint32_t kMaxChannels = 2;

// Pure layout computation. Never allocates. Returns std::nullopt when the
// request is outside policy or would exceed the caller's byte budget.
inline std::optional<Layout> layout_s16(std::uint64_t frames,
                                        std::uint32_t channels,
                                        std::uint32_t rate,
                                        std::size_t max_bytes = SIZE_MAX) noexcept {
    if (frames == 0) return std::nullopt;
    if (channels == 0 || channels > kMaxChannels) return std::nullopt;
    if (rate < kMinRate || rate > kMaxRate) return std::nullopt;

    const std::uint64_t bytes_per_frame =
        static_cast<std::uint64_t>(channels) * 2u;

    // size_t is at most 64 bits on the supported targets. The caller's
    // size_t budget also bounds addressable bytes; divide before multiplying.
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t),
                  "layout_s16 requires size_t no wider than uint64_t");
    if (frames > static_cast<std::uint64_t>(max_bytes) / bytes_per_frame)
        return std::nullopt;

    Layout out;
    out.frames = static_cast<std::size_t>(frames);
    out.bytes_per_frame = static_cast<std::size_t>(bytes_per_frame);
    out.samples = out.frames * channels;
    out.bytes = out.samples * 2u;
    out.seconds = static_cast<double>(frames) / static_cast<double>(rate);
    return out;
}

} // namespace audio_pcm

#endif // AUDIO_PCM_LAYOUT_H
