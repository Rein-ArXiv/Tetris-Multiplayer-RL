#pragma once

#include "simulation/round.h"

#include <cstdint>
#include <optional>

namespace study_game {

// 표현용 값 스냅샷. 엔진/가방/시간누적기/보류입력을 소유하지 않으며,
// 전체 경기를 복원하는 스냅샷이 아니다. 조회는 규칙 상태를 변경하지 않는다.
struct GameView {
    study_grid::Grid board;
    std::optional<study_piece::Piece> active;
    std::optional<study_ghost::Landing> ghost;
    study_next::Queue next;
    std::uint64_t score = 0;
    study_round::EndReason end_reason = study_round::EndReason::none;
};

// 실패 계약: active가 있는데 ghost가 없으면 nullopt.
// 그 외에는 값 복사본을 반환하며 포인터/참조를 저장하지 않는다.
inline std::optional<GameView> make_view(const study_round::Round& round) noexcept {
    const std::optional<study_ghost::Landing> ghost = round.ghost();  // 1회만 조회
    const std::optional<study_piece::Piece> active = round.active();
    if (active.has_value() && !ghost.has_value()) {
        return std::nullopt;
    }

    GameView view;
    view.board = round.board();
    view.active = active;
    view.ghost = ghost;
    view.next = round.next();
    view.score = round.score();
    view.end_reason = round.end_reason();
    return view;
}

}  // namespace study_game

