#pragma once

// meta/levels.h — 누적 XP → 레벨 변환 (순수 함수, 서버/클라이언트 공용).
//
// XP 는 매치로만 적립되고 절대 줄지 않는다 (meta/database.cpp 의
// kXpWin/kXpLoss — RP/BP 와 같은 트랜잭션에서 적립). 레벨은 저장하지 않고
// 항상 XP 에서 유도한다 — 곡선을 바꿔도 DB 마이그레이션이 필요 없다.
//
// 곡선: 레벨 n → n+1 에 필요한 XP 가 선형 증가 (100, 120, 140, ...).
//   레벨 60(최대) 도달 누적 = 40,120 XP ≈ 승리 100 XP 기준 약 400승.

namespace meta::levels {

constexpr int kMaxLevel = 60;

// 레벨 n 에서 n+1 로 가는 데 필요한 XP (n: 1..kMaxLevel-1).
constexpr int xp_to_next(int level)
{
    return 100 + 20 * (level - 1);
}

// 레벨 L 도달에 필요한 누적 XP. 레벨 1 = 0.
//   sum_{n=1..L-1} (100 + 20(n-1)) = 100(L-1) + 10(L-1)(L-2)
constexpr int total_xp_for_level(int level)
{
    const int k = level - 1;
    return 100 * k + 10 * k * (k - 1);
}

// 누적 XP → 현재 레벨 (1..kMaxLevel 로 clamp).
inline int level_for_xp(int xp)
{
    if (xp < 0) xp = 0;
    int level = 1;
    while (level < kMaxLevel && xp >= total_xp_for_level(level + 1))
        ++level;
    return level;
}

// 현재 레벨 안에서의 진행 XP / 다음 레벨까지 필요한 XP. UI 진행바용.
// 최대 레벨이면 둘 다 0 을 채운다 (진행바 숨김).
inline void level_progress(int xp, int& into_out, int& need_out)
{
    const int lv = level_for_xp(xp);
    if (lv >= kMaxLevel) { into_out = 0; need_out = 0; return; }
    into_out = xp - total_xp_for_level(lv);
    need_out = xp_to_next(lv);
}

} // namespace meta::levels
