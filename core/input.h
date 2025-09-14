#pragma once
#include <cstdint>

// Bitmask representing per-tick inputs
// [NET] 틱마다 입력을 비트마스크로 수집/전송하면, 
// '틱, 입력마스크'만으로 시뮬레이션을 재현할 수 있습니다(리플레이/Lockstep).
// 직렬화가 간단하고 대역폭 효율이 좋습니다.
enum InputBits : uint8_t {
    INPUT_NONE   = 0,
    INPUT_LEFT   = 1 << 0,
    INPUT_RIGHT  = 1 << 1,
    INPUT_DOWN   = 1 << 2,
    INPUT_ROTATE = 1 << 3,
    INPUT_DROP   = 1 << 4,
};

inline bool hasInput(uint8_t mask, InputBits bit) { return (mask & bit) != 0; }
