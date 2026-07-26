// placement 계산과 관측 변환의 구현. Python 쪽과 맞춰야 하는 계약은 .h에 적어 뒀다.
#include "placement.h"

#include "../src/sim_game.h"
#include "../core/input.h"

#include <algorithm>

namespace bot {

std::vector<uint8_t> expand_placement(int cur_col,
                                      int cur_rot,
                                      int tgt_col,
                                      int tgt_rot)
{
    std::vector<uint8_t> seq;
    seq.reserve(8);

    // 회전은 항상 시계 방향으로만 돈다. SimBlock에 반시계 회전이 없기 때문에
    // 목표 rotation까지 1~3번 돌리는 식으로 맞춘다.
    // UndoRotation은 "돌려보고 안 맞으면 되돌리기" 전용이라 여기선 쓸 수 없다.
    int rot_steps = ((tgt_rot - cur_rot) % kNumRotations + kNumRotations) % kNumRotations;
    for (int i = 0; i < rot_steps; ++i) {
        seq.push_back((uint8_t)INPUT_ROTATE);
    }

    if (tgt_col > cur_col) {
        int steps = tgt_col - cur_col;
        for (int i = 0; i < steps; ++i) seq.push_back((uint8_t)INPUT_RIGHT);
    } else if (tgt_col < cur_col) {
        int steps = cur_col - tgt_col;
        for (int i = 0; i < steps; ++i) seq.push_back((uint8_t)INPUT_LEFT);
    }

    seq.push_back((uint8_t)INPUT_DROP);
    return seq;
}

bool fallback_placement(const SimGame& sim, int& col_out, int& rot_out)
{
    auto placements = sim.LegalPlacements();
    if (placements.empty()) return false;

    // (col, rot) 사전순 첫 번째. 좋은 수를 찾는 게 아니라 아무거나 두는 것이다.
    auto best = std::min_element(
        placements.begin(), placements.end(),
        [](const SimGame::Placement& a, const SimGame::Placement& b) {
            if (a.col != b.col) return a.col < b.col;
            return a.rot < b.rot;
        });
    col_out = best->col;
    rot_out = best->rot;
    return true;
}

void observe(const SimGame& sim,
             float* board_out,
             float* current_out,
             float* next_out)
{
    // 굳은 블록만 1로 친다. ghost(8)는 화면에만 있는 것이라 0이다.
    // python/common/obs.py의 (grid > 0) & (grid != 8)과 같은 조건이다.
    const auto& grid = sim.Grid();
    for (int r = 0; r < kBoardRows; ++r) {
        for (int c = 0; c < kBoardCols; ++c) {
            int v = grid[r][c];
            board_out[r * kBoardCols + c] = (v > 0 && v != 8) ? 1.0f : 0.0f;
        }
    }

    // one-hot. 블록 ID는 1부터 시작하므로 인덱스는 하나씩 당긴다.
    // 범위 밖 ID가 들어오면 전부 0인 벡터가 되는데, 이는 정상 상황이 아니다.
    for (int i = 0; i < kNumPieceTypes; ++i) {
        current_out[i] = 0.0f;
        next_out[i]    = 0.0f;
    }
    int cid = sim.CurrentBlockId();
    int nid = sim.NextBlockId();
    if (cid >= 1 && cid <= kNumPieceTypes) current_out[cid - 1] = 1.0f;
    if (nid >= 1 && nid <= kNumPieceTypes) next_out[nid - 1]    = 1.0f;
}

namespace {
// 굳은 블록인지 판정한다. observe와 같은 규칙을 써야 평가와 관측이 어긋나지 않는다.
inline bool is_locked(int v) { return v > 0 && v != 8; }

// 보드를 한 숫자로 점수화한다. 클수록 좋은 판이다.
//   score = -0.51*총높이 + 0.76*삭제줄 - 0.36*구멍 - 0.18*요철
// 널리 쓰이는 Tetris 휴리스틱 가중치다. 구멍(위가 막힌 빈칸)에 큰 벌점을 주는
// 것이 핵심이고, 나머지는 판을 낮고 평평하게 유지하라는 뜻이다.
double eval_board(const int (&grid)[kBoardRows][kBoardCols], int lines_cleared)
{
    int heights[kBoardCols] = {0};
    int holes = 0;
    for (int c = 0; c < kBoardCols; ++c) {
        int top = -1;
        for (int r = 0; r < kBoardRows; ++r)
            if (is_locked(grid[r][c])) { top = r; break; }
        if (top < 0) continue;                 // 빈 컬럼
        heights[c] = kBoardRows - top;
        for (int r = top; r < kBoardRows; ++r)
            if (!is_locked(grid[r][c])) ++holes;
    }
    int agg_height = 0, bumpiness = 0;
    for (int c = 0; c < kBoardCols; ++c) agg_height += heights[c];
    for (int c = 0; c + 1 < kBoardCols; ++c) {
        int d = heights[c] - heights[c + 1];
        bumpiness += (d < 0 ? -d : d);
    }
    return -0.510066 * agg_height + 0.760666 * lines_cleared
           - 0.356630 * holes - 0.184483 * bumpiness;
}
}  // namespace

bool heuristic_placement(const SimGame& sim, int& col_out, int& rot_out)
{
    auto placements = sim.LegalPlacements();
    if (placements.empty()) return false;

    bool   found = false;
    double best  = 0.0;
    for (const auto& p : placements) {
        SimGame trial = sim;                   // 값 복사 — 실제 sim 은 불변
        int cleared = trial.ApplyPlacement(p.col, p.rot);
        if (cleared < 0) continue;             // 비합법(이론상 없음)
        double s = eval_board(trial.Grid(), cleared);
        if (!found || s > best) {
            best = s; col_out = p.col; rot_out = p.rot; found = true;
        }
    }
    return found;
}

}  // namespace bot
