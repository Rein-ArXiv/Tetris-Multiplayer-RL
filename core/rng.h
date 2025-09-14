#pragma once
#include <cstdint>

// Simple xorshift64* RNG for deterministic cross-platform randomness
// [NET] 네트코드에서 RNG는 '세션 시드'를 통해 모든 참가자가 동일하게 초기화해야 합니다.
// 블록 순서/가비지 홀/이벤트가 RNG에 의존하면, 시드와 호출 순서가 같아야 결과가 같습니다.
class XorShift64Star {
public:
    explicit XorShift64Star(uint64_t seed = 88172645463393265ull) : state(seed ? seed : 88172645463393265ull) {}

    // Next 64-bit value
    uint64_t next() {
        uint64_t x = state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state = x;
        return x * 2685821657736338717ull;
    }

    // Next unsigned int in [0, max)
    uint32_t nextUInt(uint32_t max) {
        return static_cast<uint32_t>(next() % (max ? max : 1u));
    }

    // [NET] 상태 해시/스냅샷 포함을 위해 내부 상태 접근자 제공
    uint64_t getState() const { return state; }

private:
    uint64_t state;
};
