#pragma once
#include <cstdint>
namespace study_session {
inline constexpr std::uint64_t default_seed = 0xC0FFEE123456789ull;
inline constexpr std::uint64_t garbage_tag = 0x9E3779B97F4A7C15ull;
inline constexpr std::uint64_t normalize(std::uint64_t seed) noexcept {
    return seed ? seed : default_seed;
}
// The result may be zero; the receiving engine applies its own zero policy.
inline constexpr std::uint64_t garbage_seed(std::uint64_t seed) noexcept {
    return normalize(seed) ^ garbage_tag;
}
} // namespace study_session
