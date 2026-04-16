#include "shake.h"
#include <cmath>

// XorShift64* 로컬 인스턴스 — 외부 RNG 와 상태 분리.
static uint64_t xorshift64star(uint64_t& state)
{
    uint64_t x = state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x;
    return x * 2685821657736338717ULL;
}

void shake_trigger(ShakeState& s, float intensity_px, float duration_s)
{
    if (intensity_px <= 0.0f || duration_s <= 0.0f) return;
    // 약한 흔들림이 강한 흔들림을 끊지 않도록 — 현재 활성 강도보다 약하면 무시.
    // 같은 강도면 시간만 연장.
    float curActive = (s.timeLeft > 0.0f) ? s.intensity : 0.0f;
    if (intensity_px < curActive) return;

    s.intensity = intensity_px;
    s.timeLeft  = duration_s;
    s.totalTime = duration_s;
}

void shake_update(ShakeState& s, float dt)
{
    if (s.timeLeft > 0.0f) {
        s.timeLeft -= dt;
        if (s.timeLeft <= 0.0f) {
            s.timeLeft = 0.0f;
            s.intensity = 0.0f;
        }
    }
}

void shake_offset(ShakeState& s, float& outDx, float& outDy)
{
    if (s.timeLeft <= 0.0f || s.intensity <= 0.0f || s.totalTime <= 0.0f) {
        outDx = 0.0f;
        outDy = 0.0f;
        return;
    }
    // 시간이 갈수록 진폭 감쇠 — 선형.
    float t = s.timeLeft / s.totalTime;           // 1.0 → 0.0
    float amp = s.intensity * t;

    // [-1, +1] 범위 균등 난수 두 개.
    uint64_t r1 = xorshift64star(s.rngState);
    uint64_t r2 = xorshift64star(s.rngState);
    float nx = ((float)(r1 & 0xFFFFFFu) / (float)0x800000u) - 1.0f;
    float ny = ((float)(r2 & 0xFFFFFFu) / (float)0x800000u) - 1.0f;

    outDx = amp * nx;
    outDy = amp * ny;
}
