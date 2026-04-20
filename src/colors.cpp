#include "colors.h"

// 값은 기존과 동일. Color 타입만 platform.h 의 것으로 변경.
const Color darkGrey  = { 20,  24,  44, 255};  // 보드 빈 셀 — 새 다크 배경 위에서 미세한 격자 표현
const Color green     = { 47, 230,  23, 255};
const Color red       = {232,  18,  18, 255};
const Color orange    = {226, 116,  17, 255};
const Color yellow    = {237, 234,   4, 255};
const Color purple    = {166,   0, 247, 255};
const Color cyan      = { 21, 204, 209, 255};
const Color blue      = { 13,  64, 216, 255};
const Color lightBlue = { 59,  85, 162, 255};
const Color darkBlue  = { 44,  44, 127, 255};
const Color gray      = {127, 127, 127, 255};
const Color garbageColor = { 80,  80,  90, 255};  // id=9 — 가비지 셀 (어두운 회색)
// id=8 — 고스트 블록: 반투명 흰회색 (알파 70/255 ≈ 27%)
const Color ghostColor   = {200, 200, 210,  70};

std::vector<Color> GetCellColors()
{
    return {darkGrey, green, red, orange, yellow, purple, cyan, blue, ghostColor, garbageColor};
}
