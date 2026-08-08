#pragma once
#include <cstdint>
#include <vector>

// 인게임 bot이 쓰는 placement 계산과 관측 변환.
//
// 같은 로직이 Python 쪽(python/netbot/input_expander.py, python/common/obs.py)에도
// 있다. 학습은 Python에서 하고 추론은 여기서 하므로 두 구현이 어긋나면
// 학습한 정책이 게임 안에서 다르게 행동한다. test_placement_parity가 이 둘을
// 대조하니, 한쪽을 고치면 반드시 다른 쪽도 같이 고쳐야 한다.
//
//   expand_placement — "3번 열에 2번 회전해서 놓기" 같은 placement를
//                      tick 단위 input mask 시퀀스로 풀어낸다.
//                      bot도 사람과 똑같이 키 입력으로만 게임을 움직인다.
//
//   observe          — SimGame을 신경망 입력 텐서로 바꾼다.
//                      board(20x10), current(7), next(7) 전부 float32.
//
//   fallback_placement — 추론이 실패했을 때 쓰는 최후 수단.
//   heuristic_placement — ONNX 모델이 없을 때 쓰는 규칙 기반 bot.

class SimGame;

namespace bot {

// action 인덱스는 col * kNumRotations + rot 로 매긴다.
// Python 쪽과 같은 규칙이어야 학습한 정책의 출력이 그대로 통한다.
constexpr int kNumCols       = 10;
constexpr int kNumRotations  = 4;
constexpr int kNumPlacements = kNumCols * kNumRotations;  // 40
constexpr int kNumPieceTypes = 7;
constexpr int kBoardRows     = 20;
constexpr int kBoardCols     = 10;

// placement를 tick별 input mask 시퀀스로 푼다.
// 회전 먼저((tgt_rot - cur_rot) % 4 번), 그다음 좌우 이동, 마지막에 hard drop.
// 순서가 중요하다 — 회전하면 블록의 폭이 바뀌어서 이동 가능 범위도 달라진다.
// 반환값의 각 원소는 core/input.h의 INPUT_* 비트마스크다.
std::vector<uint8_t> expand_placement(int cur_col,
                                      int cur_rot,
                                      int tgt_col,
                                      int tgt_rot);

// 아무 합법 수나 하나 고른다. (col, rot) 오름차순의 첫 번째.
// 좋은 수를 두려는 게 아니라 bot이 멈춰버리는 것을 막는 안전장치다.
// 합법 수가 하나도 없으면(= 게임 오버 직전) false.
bool fallback_placement(const SimGame& sim, int& col_out, int& rot_out);

// 1수 앞만 보는 greedy policy. 합법 수를 전부 SimGame 복사본에 둬 보고
// 결과 보드를 점수화해 제일 나은 것을 고른다.
// 평가 항목은 총 높이, 지운 줄 수, 구멍 수, 요철 네 가지다.
// 학습 모델 없이도 돌아가므로 ONNX Runtime이 없는 빌드의 기본 bot이 된다.
// 합법 수가 없으면 false.
bool heuristic_placement(const SimGame& sim, int& col_out, int& rot_out);

// 신경망 입력 텐서를 채운다. 세 버퍼 모두 호출자가 미리 할당해서 넘긴다.
//   board_out   — 200칸(20x10). 굳은 블록이 있으면 1, 아니면 0.
//                 ghost 블록(id=8)은 화면 표시용이라 0으로 친다.
//   current_out — 현재 블록 종류의 one-hot 7칸.
//   next_out    — 다음 블록 종류의 one-hot 7칸.
void observe(const SimGame& sim,
             float* board_out,
             float* current_out,
             float* next_out);

// action 인덱스 <-> (col, rot) 변환.
inline int  encode_action(int col, int rot) { return col * kNumRotations + rot; }
inline void decode_action(int action, int& col, int& rot)
{
    col = action / kNumRotations;
    rot = action % kNumRotations;
}

}  // namespace bot
