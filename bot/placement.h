#pragma once
#include <cstdint>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// bot/placement.h — C++ port of python/netbot/input_expander.py + obs builder.
//
// 두 가지 역할:
//   1) expand_placement  : (cur_col, cur_rot) → (tgt_col, tgt_rot) 이동을 위한
//                          틱별 INPUT 비트마스크 시퀀스 + 마지막에 INPUT_DROP.
//                          로직은 Python 원본(input_expander.expand_placement)
//                          과 바이트 단위로 동일하게 맞춤 — test_placement_parity
//                          가 양쪽 결과를 교차 검증.
//   2) observe           : SimGame 을 NN 입력 텐서 레이아웃으로 변환.
//                          board(1,20,10) + current(7) + next(7), float32.
//                          python/common/obs.build_observation 과 동일.
//
// 부가:
//   fallback_placement — LegalPlacements 중 (col, rot) 사전순 최솟값 선택.
//                        policy 가 실패/비합법 액션을 돌려줄 때 쓰는 안전망.
// ─────────────────────────────────────────────────────────────────────────────

class SimGame;

namespace bot {

// Python 과 동일 상수. placement action = col * kNumRotations + rot.
constexpr int kNumCols       = 10;
constexpr int kNumRotations  = 4;
constexpr int kNumPlacements = kNumCols * kNumRotations;  // 40
constexpr int kNumPieceTypes = 7;
constexpr int kBoardRows     = 20;
constexpr int kBoardCols     = 10;

// Python expand_placement 와 동일한 시퀀스를 돌려준다.
//   rot_steps = (tgt_rot - cur_rot) % 4 회 INPUT_ROTATE,
//   그다음 |tgt_col - cur_col| 회 INPUT_LEFT 또는 INPUT_RIGHT,
//   마지막에 INPUT_DROP.
// 원소는 uint8_t — core/input.h 의 INPUT_* 비트 그대로.
std::vector<uint8_t> expand_placement(int cur_col,
                                      int cur_rot,
                                      int tgt_col,
                                      int tgt_rot);

// LegalPlacements 를 (col, rot) 오름차순으로 훑어 첫 원소 반환.
// 합법 수가 없으면 false.
bool fallback_placement(const SimGame& sim, int& col_out, int& rot_out);

// 1-ply 그리디 휴리스틱 (El-Tetris 가중치: 높이/줄/구멍/요철). 각 합법 placement 를
// SimGame 복사본에 적용해 결과 보드를 평가하고 최고 점수를 고른다. ONNX/모델 없이
// 동작하는 테스트용 봇 — netbot 의 BCTS RuleBasedRunner 의 C++ 간이판.
// 합법 수가 없으면 false.
bool heuristic_placement(const SimGame& sim, int& col_out, int& rot_out);

// board_out: 길이 kBoardRows * kBoardCols (=200) float32. 점유=1, 비점유=0.
//            ghost(id=8) 와 empty(0) 는 0 으로, 그 외 locked 는 1.
// current_out, next_out: 길이 kNumPieceTypes (=7) float32 one-hot.
// 세 버퍼 모두 호출자가 할당·소유. obs_board_size() 등 헬퍼 없이 상수 직접 사용.
void observe(const SimGame& sim,
             float* board_out,
             float* current_out,
             float* next_out);

// action index <-> (col, rot) 왕복. Python action_mask.encode/decode 와 동일.
inline int  encode_action(int col, int rot) { return col * kNumRotations + rot; }
inline void decode_action(int action, int& col, int& rot)
{
    col = action / kNumRotations;
    rot = action % kNumRotations;
}

}  // namespace bot
