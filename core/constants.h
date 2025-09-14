#pragma once

// Simulation tick rate (logic updates per second)
// [NET] Lockstep/rollback 네트코드에서 '틱'은 동기 단위입니다.
// 모든 피어/서버가 동일한 틱 카운터를 기준으로 
// 같은 입력을 같은 순서로 적용해야 결정론이 보장됩니다.
constexpr int TICKS_PER_SECOND = 60;
constexpr float SECONDS_PER_TICK = 1.0f / static_cast<float>(TICKS_PER_SECOND);
