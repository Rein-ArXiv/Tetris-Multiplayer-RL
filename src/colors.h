#pragma once
#include <vector>
#include "../platform/platform.h"   // Color 정의 (raylib Color 대체)

// 테트리스 블록 색상
extern const Color darkGrey;
extern const Color green;
extern const Color red;
extern const Color orange;
extern const Color yellow;
extern const Color purple;
extern const Color cyan;
extern const Color blue;
extern const Color lightBlue;
extern const Color darkBlue;
extern const Color gray;

// 셀 인덱스(0~8) → Color 매핑 반환. SimGame::Grid()[row][col] 값이 인덱스.
std::vector<Color> GetCellColors();
