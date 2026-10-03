#pragma once
#include "renderer/rgba_pixels.h"
#include <filesystem>
#include <optional>
#include <vector>
namespace study_image {
inline constexpr std::size_t max_encoded_bytes = 8u*1024u*1024u;
inline constexpr std::size_t max_decoded_bytes = 64u*1024u*1024u;
inline constexpr int max_side = 8192;
enum class Error { none, invalid_input, encoded_limit, file_io,
                   decode_failed, dimensions, allocation };
const char* error_text(Error error) noexcept;
struct Image {
    int width=0, height=0, source_channels=0;
    std::vector<std::uint8_t> pixels;
    study_texture::RgbaView view() const noexcept {
        return {pixels.data(),pixels.size(),width,height};
    }
};
struct Result {
    std::optional<Image> image;
    Error error=Error::none;
    explicit operator bool() const noexcept { return image.has_value(); }
};
// Input bytes must stay readable and unchanged throughout the call.
// Asset contract: standard PNG/JPEG, decoded top-down to packed RGBA8, straight alpha.
// Nonstandard variants such as CgBI are outside this contract; the decoder is
// not a strict PNG conformance validator.
// Limits bound accepted input/output, not total decoder working memory or time.
Result decode_memory(const std::uint8_t* bytes, std::size_t size) noexcept;
Result decode_file(const std::filesystem::path& path) noexcept;
} // namespace study_image
