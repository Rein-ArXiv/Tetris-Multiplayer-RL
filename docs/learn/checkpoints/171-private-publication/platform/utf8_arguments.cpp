#include "utf8_arguments.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <cwchar>
#include <limits>
#include <stdexcept>
#include <utility>

namespace platform {
namespace {

// Fixed, non-sensitive error text. Never includes argument content.
constexpr const char* kConversionError =
    "failed to convert command line argument to UTF-8";

}  // namespace

std::vector<std::string> utf8_arguments(int argc, const wchar_t* const* argv) {
    if (argc < 0) {
        throw std::runtime_error(kConversionError);
    }
    if (argc > 0 && argv == nullptr) {
        throw std::runtime_error(kConversionError);
    }

    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc));

    for (int i = 0; i < argc; ++i) {
        const wchar_t* const arg = argv[i];
        if (arg == nullptr) {
            throw std::runtime_error(kConversionError);
        }

        const std::size_t wide_length = std::wcslen(arg);
        if (wide_length > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            throw std::runtime_error(kConversionError);
        }
        // Explicit source length excludes the terminating NUL.
        const int source_length = static_cast<int>(wide_length);

        // Empty argument must become an empty string.
        if (source_length == 0) {
            result.emplace_back();
            continue;
        }

        // First call: size the UTF-8 output.
        const int utf8_length = ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, arg, source_length,
            nullptr, 0, nullptr, nullptr);
        if (utf8_length <= 0) {
            throw std::runtime_error(kConversionError);
        }

        std::string converted(static_cast<std::size_t>(utf8_length), '\0');

        // Second call: perform the conversion into the exact buffer size.
        const int written = ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, arg, source_length,
            converted.data(), utf8_length, nullptr, nullptr);
        if (written != utf8_length) {
            throw std::runtime_error(kConversionError);
        }

        result.push_back(std::move(converted));
    }

    return result;
}

}  // namespace platform

#endif  // _WIN32
