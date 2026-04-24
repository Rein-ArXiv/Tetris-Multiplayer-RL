#pragma once

// meta/elo.h — ELO 레이팅 계산 (순수 함수).
//
// K-factor 는 세 단계 (<1200/1800/>=1800) 로 구분해 초보자는 빠르게,
// 고수는 천천히 변동하게 한다. 이는 FIDE/USCF 관행을 단순화한 버전.
//
// expected(ra, rb) = 1 / (1 + 10^((rb - ra) / 400))
// new_r = r + K * (score - expected)    (승=1, 패=0)

#include <algorithm>
#include <cmath>
#include <utility>

namespace elo {

inline int k_factor(int rating)
{
    if (rating < 1200) return 32;
    if (rating < 1800) return 24;
    return 16;
}

inline double expected(int ra, int rb)
{
    return 1.0 / (1.0 + std::pow(10.0, (rb - ra) / 400.0));
}

// 승자/패자 쌍의 새 ELO 를 반환.  ELO 는 100 아래로 내려가지 않도록 clamp.
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
        std::max(100, new_winner),
        std::max(100, new_loser),
    };
}

} // namespace elo
