#pragma once

// src/gui.h — Immediate-mode GUI 위젯
//
// 이 위젯 함수들은 체크 상태나 메뉴 선택을 소유하지 않는다. 호출자가 값을
// 보관하고 매 프레임 현재 값과 입력으로 UI를 기술한다. 플랫폼의 입력 이력과
// 렌더러의 배치는 별도로 상태를 가진다. 즉시모드 UI 일반이 무상태라는 뜻은 아니다.
// 이 구현은 위젯 객체 트리 없이 그리기와 현재 입력 판정을 함께 호출한다.
//
// 사용 예:
//   if (gui_button(180, 230, 200, 40, "Single Play")) { /* 클릭됨 */ }
//
// 키보드 병행:
//   - 위젯 자체는 마우스 전담이다. 메뉴 상하 커서 이동과 Enter 는 호출부에서
//     별도 처리 (gui 는 그 키를 해석하지 않는다).
//   - "강조된 항목" 을 기호로 그려야 할 땐 gui_button_highlighted 를 쓴다.

#include "../platform/platform.h"

// 논리 좌표의 반열린 박스 [x,x+w) × [y,y+h). 양수가 아닌 크기는 거절한다.
bool gui_hover_rect(int x, int y, int w, int h);

// 사각형 버튼. 클릭되면 true (mouse 좌클릭 pressed 엣지).
//   - hover 시 밝게, 버튼을 누르고 있는 동안에는 눌림 색을 사용한다.
//   - 키보드로는 반응 안 함 (호출부가 별도 처리).
bool gui_button(int x, int y, int w, int h, const char* label, int fontSize = 24);

// 변형: 현재 메뉴 커서가 가리키는 항목처럼 강조 색으로 그림. 여전히 클릭도 받는다.
bool gui_button_highlighted(int x, int y, int w, int h, const char* label,
                            bool highlighted, int fontSize = 24);

// 우상단 X 모양 아이콘 버튼. 인게임 "나가기" 버튼용.
bool gui_close_button(int x, int y, int size);

// 체크박스 + 라벨. (x,y) 좌상단, size = 박스 한 변(px).
//   checked: 채워진 박스 vs 빈 박스로 그림.
//   highlighted: 키보드 커서 강조 (gui_button_highlighted 와 동일 철학).
//   반환: 좌클릭 엣지(true) — 호출부가 bool 을 토글한다.
//   size < 4 또는 파생 좌표/폭이 int 범위를 벗어나면 그리지 않고 false.
bool gui_checkbox(int x, int y, int size, const char* label, bool checked,
                  bool highlighted = false);

// 가로 슬라이더 (0~100%). 트랙 + 채워진 구간 + 노브를 그린다.
//   valuePct: 현재 값(0~100). highlighted: 키보드 커서 강조.
//   반환: 트랙 위를 좌클릭/드래그 중이면 그 위치에 해당하는 새 값(0~100),
//         조작이 없으면 valuePct 를 그대로 반환. 호출부는 반환값을 저장한다.
int  gui_slider(int x, int y, int w, int h, int valuePct, bool highlighted);

// 값 선택기: "◀ label ▶" 형태. 좌/우 화살표 버튼을 클릭하면 -1/+1 을 반환.
//   (x,y) 좌상단, w/h 전체 박스 크기. label 은 중앙 표시(예: "1080x960").
//   반환: 왼쪽 화살표 클릭 = -1, 오른쪽 = +1, 그 외 0. highlighted = 커서 강조.
//   allowPrevious/allowNext가 false인 화살표는 숨기고 입력도 받지 않는다.
//   h > 6, w > 2*h와 int 표현 가능한 배치가 필요하다. 실패하면 그리지 않고 0.
int  gui_value_selector(int x, int y, int w, int h, const char* label,
                        bool highlighted, bool allowPrevious = true, bool allowNext = true);

// 모달 배경(반투명 오버레이)을 전체 화면에 덮는다. 모달 창은 위에 겹쳐 그린다.
// 화면 크기는 main.cpp 가 안다 → 인자로 받음.
void gui_modal_dim(int screenW, int screenH);

// 텍스트 수평 중앙 정렬로 한 줄 그리기 헬퍼.
void gui_text_center(int centerX, int y, const char* text, int fontSize, Color c);
