#ifndef STUDY_UTF8_HPP
#define STUDY_UTF8_HPP

// study_utf8: minimal UTF-8 decoding for one code point at a time.
//
// decode_first(input):
//   * empty input -> Result{} (codepoint 0, bytes 0, Status::end).
//   * otherwise consumes 1..4 bytes, never more than input.size().
//   * malformed, truncated, overlong or non-scalar sequences yield
//     U+FFFD with exactly one byte consumed and Status::invalid; this is a
//     per-byte replacement policy, not maximal-subpart, so a later ASCII
//     byte is never swallowed.
//   * Status::scalar marks a real scalar value; an embedded U+0000 is a
//     scalar, which keeps it distinct from Status::end.
//   * Status::invalid marks failure and carries U+FFFD; a genuine U+FFFD
//     in the input is reported as Status::scalar.
#include <climits>
#include <cstddef>
#include <cstdint>
#include <string_view>

static_assert(CHAR_BIT == 8, "study_utf8 requires 8-bit bytes");

namespace study_utf8 {

enum class Status { end, scalar, invalid };

struct Result {
    char32_t codepoint = 0;
    std::size_t bytes = 0;
    Status status = Status::end;
};

namespace detail {

constexpr char32_t kMaxCodepoint = 0x10FFFF;
constexpr char32_t kSurrogateLow = 0xD800;
constexpr char32_t kSurrogateHigh = 0xDFFF;

// True when byte is 10xxxxxx.
constexpr bool is_continuation(unsigned char byte) noexcept {
    return (byte & 0xC0u) == 0x80u;
}

// Decode the length-byte sequence in input; caller guarantees size() >= length.
inline Result assemble(std::string_view input, std::size_t length,
                       std::uint32_t lead_mask, char32_t minimum) noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(
        static_cast<unsigned char>(input[0]) & lead_mask);
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char byte = static_cast<unsigned char>(input[i]);
        if (!is_continuation(byte)) {
            return {0xFFFD, 1, Status::invalid};
        }
        value = (value << 6) | (static_cast<std::uint32_t>(byte) & 0x3Fu);
    }
    const char32_t code = static_cast<char32_t>(value);
    if (code < minimum || code > kMaxCodepoint ||
        (code >= kSurrogateLow && code <= kSurrogateHigh)) {
        return {0xFFFD, 1, Status::invalid};
    }
    return {code, length, Status::scalar};
}

}  // namespace detail

inline Result decode_first(std::string_view input) noexcept {
    if (input.empty()) {
        return Result{};
    }

    const unsigned char lead = static_cast<unsigned char>(input[0]);

    if (lead <= 0x7F) {
        // 00..7F, including an embedded U+0000, is a 1-byte scalar.
        return {static_cast<char32_t>(lead), 1, Status::scalar};
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        // 2-byte sequence; C0/C1 would encode below U+0080.
        if (input.size() < 2) {
            return {0xFFFD, 1, Status::invalid};
        }
        return detail::assemble(input, 2, 0x1Fu, 0x80);
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        // 3-byte sequence; overlong and surrogate results are rejected below.
        if (input.size() < 3) {
            return {0xFFFD, 1, Status::invalid};
        }
        return detail::assemble(input, 3, 0x0Fu, 0x800);
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        // 4-byte sequence; F4 results above U+10FFFF are rejected below.
        if (input.size() < 4) {
            return {0xFFFD, 1, Status::invalid};
        }
        return detail::assemble(input, 4, 0x07u, 0x10000);
    }
    // 80..BF is a stray continuation; C0/C1 and F5..FF are never valid leads.
    return {0xFFFD, 1, Status::invalid};
}

}  // namespace study_utf8

#endif  // STUDY_UTF8_HPP
