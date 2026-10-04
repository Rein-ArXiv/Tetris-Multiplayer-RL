#ifndef AUDIO_MP3_DECODE_H
#define AUDIO_MP3_DECODE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace audio_mp3 {

// Format failures vs. resource failures are reported through distinct values.
enum class Error {
    none = 0,
    invalid_argument,   // null/empty path, zero budget, bad parameters
    io,                 // fopen/fseek/fread/ferror failure or file growth
    empty,              // zero-length input or zero-length file
    file_too_large,     // compressed input exceeds Limits::file_bytes
    decode_failed,      // backend produced no frames / unusable stream
    unsupported_format, // channel/rate layout rejected by pcm_layout
    pcm_too_large,      // decoded PCM exceeds Limits::pcm_bytes
    allocation_failed   // std::bad_alloc / std::length_error
};

// Byte budgets. file_bytes caps the compressed input; pcm_bytes caps the
// decoded interleaved 16-bit output. The defaults permit the bundled assets.
// These are not a process-wide memory or CPU guarantee: vector capacity
// and temporary reallocation overhead differ.
struct Limits {
    std::size_t file_bytes = 16u * 1024u * 1024u;
    std::size_t pcm_bytes  = 64u * 1024u * 1024u;
};

struct Result {
    Error error = Error::none;
    uint32_t channels = 0;
    uint32_t rate = 0;
    std::vector<int16_t> samples; // interleaved, complete frames only

    explicit operator bool() const noexcept {
        return error == Error::none && !samples.empty();
    }
};

// Decodes interleaved signed 16-bit PCM from an in-memory MP3 image.
// The input buffer is borrowed and must remain alive for the call.
Result decode(const std::vector<uint8_t>& input, Limits limits = {}) noexcept;

// Reads a file (checked against the compressed budget before allocation)
// and decodes it. Never partially succeeds on a cap excess.
Result load(const char* path, Limits limits = {}) noexcept;

// Stable, human-readable name for an Error value (never null).
const char* error_name(Error e) noexcept;

} // namespace audio_mp3

#endif // AUDIO_MP3_DECODE_H
