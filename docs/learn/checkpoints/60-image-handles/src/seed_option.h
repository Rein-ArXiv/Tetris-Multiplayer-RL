#pragma once
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
namespace study_seed {
// Decimal only, including zero/leading zeros. Reject signs, spaces and overflow.
inline std::optional<std::uint64_t> parse(std::string_view text) noexcept {
    if (text.empty()) return std::nullopt;
    for (const char ch : text) if (ch < '0' || ch > '9') return std::nullopt;
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return std::nullopt;
    return value;
}
} // namespace study_seed
