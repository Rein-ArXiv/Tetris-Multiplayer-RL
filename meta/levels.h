#pragma once

// meta/levels.h — 누적 XP → 레벨 변환 (순수 함수, 서버/클라이언트 공용).
//
// 매치 정산에서는 XP를 RP/BP와 같은 트랜잭션에서 적립한다.
// 레벨은 저장하지 않고 누적 XP에서 유도한다. 곡선을 바꾸면 같은 XP의
// 표시 레벨도 달라지므로 기존 이용자에 대한 적용 정책을 함께 정해야 한다.
//
// 곡선: 레벨 n → n+1 에 필요한 XP 가 선형 증가 (100, 120, 140, ...).
//   레벨 60(최대) 도달 누적 = 40,120 XP ≈ 승리 100 XP 기준 402승.

namespace meta::levels {

constexpr int kMaxLevel = 60;
constexpr int kBaseCost = 100;
constexpr int kCostStep = 20;

// 표시용 레벨 입력은 산술 전에 범위로 제한한다.
constexpr int bounded_level(int level)
{
    return level < 1 ? 1 : (level > kMaxLevel ? kMaxLevel : level);
}

// 레벨을 1..60으로 제한한다. 최대 레벨에는 다음 단계가 없어 0을 반환한다.
constexpr int xp_to_next(int level)
{
    level = bounded_level(level);
    return level == kMaxLevel ? 0 : kBaseCost + kCostStep * (level - 1);
}

// 레벨 L 도달에 필요한 누적 XP. 레벨 1 = 0.
//   sum_{n=1..L-1} (100 + 20(n-1)) = 100(L-1) + 10(L-1)(L-2)
constexpr int total_xp_for_level(int level)
{
    const int k = bounded_level(level) - 1;
    return kBaseCost * k + kCostStep * k * (k - 1) / 2;
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
    if (xp < 0) xp = 0;
    const int lv = level_for_xp(xp);
    if (lv >= kMaxLevel) { into_out = 0; need_out = 0; return; }
    into_out = xp - total_xp_for_level(lv);
    need_out = xp_to_next(lv);
}

} // namespace meta::levels
