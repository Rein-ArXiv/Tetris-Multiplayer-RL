#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace relay {

inline constexpr char kRoomCodeAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
inline constexpr std::size_t kRoomCodeLength = 5;
static_assert(sizeof(kRoomCodeAlphabet) - 1 == 32);

// Five low-to-high groups of five bits. All-zero input is the valid code AAAAA.
// Uniform 32-bit input gives uniform 25-bit codes; the upper seven bits are unused.
inline std::string roomCodeFromWord(std::uint32_t word) {
    std::string code(kRoomCodeLength, 'A');
    for (char& c : code) {
        c = kRoomCodeAlphabet[word & 31u];
        word >>= 5;
    }
    return code;
}

// Fresh OS random bytes per candidate. Failure returns nullopt; no clock/PRNG fallback.
std::optional<std::uint32_t> roomCodeRandomWord() noexcept;

// Caller must serialize this search AND its subsequent insertion against other
// changes to the same registry. A returned candidate is not itself a reservation.
// Injection separates collision policy from the OS source for deterministic tests.
template <class Next, class Occupied>
std::optional<std::string> selectRoomCode(Next&& next, Occupied&& occupied) {
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        const auto word = next();
        if (!word) return std::nullopt;
        auto code = roomCodeFromWord(*word);
        if (!occupied(code)) return code;
    }
    return std::nullopt;
}

} // namespace relay
