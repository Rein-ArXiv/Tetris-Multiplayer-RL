#pragma once
#include <cstdint>

// Bitmask representing per-tick inputs
// 같은 규칙 버전·초기 상태·시드·틱별 입력 순서가 재현의 전제다.
// 마스크는 요청의 집합이며, 처리 순서는 SubmitInput이 정한다.
enum InputBits : uint8_t {
    INPUT_NONE   = 0,
    INPUT_LEFT   = 1 << 0,
    INPUT_RIGHT  = 1 << 1,
    INPUT_DOWN   = 1 << 2,
    INPUT_ROTATE = 1 << 3,
    INPUT_DROP   = 1 << 4,
};

inline constexpr uint8_t INPUT_KNOWN_MASK =
    INPUT_LEFT | INPUT_RIGHT | INPUT_DOWN | INPUT_ROTATE | INPUT_DROP;

// Validate wide parsed values BEFORE narrowing to uint8_t. Zero is valid.
// Both direction bits are allowed; their meaning belongs to the simulation.
inline constexpr bool isValidInputMask(uint64_t value) noexcept {
    return (value & ~uint64_t(INPUT_KNOWN_MASK)) == 0;
}

// Tests overlap; INPUT_NONE (zero) is never reported as present.
inline constexpr bool hasInput(uint8_t mask, InputBits bit) noexcept {
    return (mask & bit) != 0;
}
