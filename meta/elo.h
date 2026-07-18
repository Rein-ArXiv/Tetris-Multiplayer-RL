#pragma once

// meta/elo.h — RP(Rating Point) 계산 (순수 함수).
//
// 수식은 표준 ELO 그대로지만 스케일을 게임 친화적으로 리베이스했다:
//   · 시작 0, 바닥 0 — 신규 플레이어가 "0 RP" 에서 출발해 위로만 쌓는 표기.
//     바닥(0)에서는 패배해도 더 잃지 않는다 (일반적인 래더 관행).
//   · ELO 의 기대승률은 두 레이팅의 *차이* 만 쓰므로 기준점 이동은 수학적으로
//     무손실이다. (구 스케일 1200 시작 → 신 스케일 0 시작; 기존 DB 는
//     database.cpp 의 1회성 마이그레이션이 elo-1200 으로 이관)
//
// K-factor 는 세 단계 (<300 / <600 / >=600) — 하위 구간은 빠르게 수렴,
// 상위는 천천히 변동. FIDE/USCF 관행의 리베이스판.
//
// expected(ra, rb) = 1 / (1 + 10^((rb - ra) / 400))
// new_r = r + K * (score - expected)    (승=1, 패=0)
//
// 내부 식별자/DB 컬럼명은 호환을 위해 `elo` 를 유지한다. UI 표기만 "RP".

#include <algorithm>
#include <cmath>
#include <utility>

namespace elo {

inline int k_factor(int rating)
{
    if (rating < 300) return 32;
    if (rating < 600) return 24;
    return 16;
}

inline double expected(int ra, int rb)
{
    return 1.0 / (1.0 + std::pow(10.0, (rb - ra) / 400.0));
}

// 승자/패자 쌍의 새 RP 를 반환. 0 아래로 내려가지 않도록 clamp.
struct Update {
    int new_winner;
    int new_loser;
};

inline Update update(int winner_elo, int loser_elo)
{
    const double e_win = expected(winner_elo, loser_elo);
    const double e_los = expected(loser_elo, winner_elo);

    const int new_winner = winner_elo + static_cast<int>(std::round(
        k_factor(winner_elo) * (1.0 - e_win)));
    const int new_loser  = loser_elo  + static_cast<int>(std::round(
        k_factor(loser_elo)  * (0.0 - e_los)));

    return {
        std::max(0, new_winner),
        std::max(0, new_loser),
    };
}

} // namespace elo
