#define DR_MP3_IMPLEMENTATION
#include "../third_party/dr_mp3.h"

#include "mp3_decode.h"
#include "pcm_layout.h"

#include <array>
#include <cstdio>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace audio_mp3 {
namespace {

// RAII wrapper for the C stream used by load(). Closes on every exit path.
struct FileCloser {
    void operator()(std::FILE* f) const noexcept {
        if (f != nullptr) {
            std::fclose(f);
        }
    }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

} // namespace

const char* error_name(Error e) noexcept {
    switch (e) {
        case Error::none:               return "none";
        case Error::invalid_argument:   return "invalid_argument";
        case Error::io:                 return "io";
        case Error::empty:              return "empty";
        case Error::file_too_large:     return "file_too_large";
        case Error::decode_failed:      return "decode_failed";
        case Error::unsupported_format: return "unsupported_format";
        case Error::pcm_too_large:      return "pcm_too_large";
        case Error::allocation_failed:  return "allocation_failed";
    }
    return "unknown";
}

Result decode(const std::vector<uint8_t>& input, Limits limits) noexcept {
    Result result;

    // Resource budgets are checked before touching the decoder.
    if (limits.file_bytes == 0u || limits.pcm_bytes == 0u) {
        result.error = Error::invalid_argument;
        return result;
    }
    if (input.empty()) {
        result.error = Error::empty;
        return result;
    }
    if (input.size() > limits.file_bytes) {
        result.error = Error::file_too_large;
        return result;
    }

    // drmp3_init_memory borrows the input image: it is not copied here and
    // must stay alive for the whole decode (it is a const reference).
    drmp3 decoder{};
    if (!drmp3_init_memory(&decoder, input.data(), input.size(), nullptr)) {
        result.error = Error::decode_failed;
        return result;
    }

    // Uninit runs only after a successful init, on every exit path.
    struct Guard {
        drmp3* d;
        ~Guard() { drmp3_uninit(d); }
    } guard{&decoder};

    const uint32_t channels = static_cast<uint32_t>(decoder.channels);
    const uint32_t rate = static_cast<uint32_t>(decoder.sampleRate);

    try {
        // Metadata validity is independent of the PCM byte budget. An empty
        // optional means the channel/rate layout is unsupported or would
        // overflow, i.e. a format failure rather than a resource failure.
        if (!audio_pcm::layout_s16(1, channels, rate)) {
            result.error = Error::unsupported_format;
            return result;
        }

        // pcm_bytes counts output bytes; samples are 16-bit (2 bytes each).
        const std::size_t maxSamples = limits.pcm_bytes / 2u;
        const std::size_t blockFrames =
            4096u / channels;
        const drmp3_uint64 askFrames = static_cast<drmp3_uint64>(blockFrames);

        std::array<drmp3_int16, 4096> block{};

        for (;;) {
            // The backend returns frames, not scalar samples. A zero return
            // ends iteration: the library uses permissive prefix semantics
            // and makes no claim of validating full MP3 integrity.
            const drmp3_uint64 got =
                drmp3_read_pcm_frames_s16(&decoder, askFrames, block.data());
            if (got == 0u) {
                break;
            }
            if (got > askFrames) {
                result.error = Error::decode_failed; // contract violation
                return Result{result.error, 0, 0, {}};
            }

            const std::size_t added =
                static_cast<std::size_t>(got) * static_cast<std::size_t>(channels);

            // Reject BEFORE appending. size() <= maxSamples is an invariant,
            // so the subtraction cannot underflow. No partial success is
            // reported if the cap is exceeded, even after a valid prefix.
            if (added > maxSamples - result.samples.size()) {
                result.error = Error::pcm_too_large;
                return Result{result.error, 0, 0, {}};
            }

            // Growth is left to the vector; this file does not promise any
            // peak-memory bound, only the final sample-count budget above.
            result.samples.insert(result.samples.end(),
                                  block.data(),
                                  block.data() + added);
        }
    } catch (const std::bad_alloc&) {
        result.error = Error::allocation_failed;
        return Result{result.error, 0, 0, {}};
    } catch (const std::length_error&) {
        result.error = Error::allocation_failed;
        return Result{result.error, 0, 0, {}};
    }

    if (result.samples.empty()) {
        result.error = Error::decode_failed;
        return result;
    }

    // Output metadata is the decoder's fixed init-time format in the vendored
    // version; it does not mutate mid-stream, so no manual fixups are applied.
    result.channels = channels;
    result.rate = rate;
    result.error = Error::none;
    return result;
}

Result load(const char* path, Limits limits) noexcept {
    Result result;

    if (path == nullptr || path[0] == '\0') {
        result.error = Error::invalid_argument;
        return result;
    }
    if (limits.file_bytes == 0u || limits.pcm_bytes == 0u) {
        result.error = Error::invalid_argument;
        return result;
    }

    FilePtr file(std::fopen(path, "rb"));
    if (!file) {
        result.error = Error::io;
        return result;
    }

    if (std::fseek(file.get(), 0, SEEK_END) != 0) {
        result.error = Error::io;
        return result;
    }

    const long endPos = std::ftell(file.get());
    if (endPos < 0) {
        result.error = Error::io;
        return result;
    }

    // Compare as unsigned 64-bit before any size_t narrowing.
    const uint64_t fileSize = static_cast<uint64_t>(endPos);
    if (fileSize == 0u) {
        result.error = Error::empty;
        return result;
    }
    if (fileSize > static_cast<uint64_t>(limits.file_bytes)) {
        result.error = Error::file_too_large; // reject before allocating
        return result;
    }

    if (std::fseek(file.get(), 0, SEEK_SET) != 0) {
        result.error = Error::io;
        return result;
    }

    std::vector<uint8_t> input;
    try {
        input.resize(static_cast<std::size_t>(fileSize));
    } catch (const std::bad_alloc&) {
        result.error = Error::allocation_failed;
        return result;
    } catch (const std::length_error&) {
        result.error = Error::allocation_failed;
        return result;
    }

    const std::size_t got = std::fread(input.data(), 1, input.size(), file.get());
    if (got != input.size()) {
        result.error = Error::io;
        return result;
    }

    // Detect a file that grew between ftell and fread, plus any read error.
    const int extra = std::fgetc(file.get());
    if (extra != EOF || std::ferror(file.get())) {
        result.error = Error::io;
        return result;
    }

    // decode() is noexcept and owns its allocation handling, so no extra
    // catch is required. No path partially succeeds: each cap violation
    // yields empty samples plus a distinct error.
    return decode(input, limits);
}

} // namespace audio_mp3
