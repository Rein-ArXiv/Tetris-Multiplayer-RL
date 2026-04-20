#pragma once

// src/gui.h — Immediate-mode GUI 위젯
//
// 상태를 저장하지 않는 함수형 위젯 모음. 매 프레임 그리면서 그 프레임의
// 마우스·키보드 입력과 직접 비교해 hover/click 을 판정한다.
// Dear ImGui 패턴과 동일한 철학: 별도 레이아웃 트리나 위젯 객체가 없어서
// 기존 렌더 루프에 그냥 끼워넣으면 된다.
//
// 사용 예:
//   if (gui_button(180, 230, 200, 40, "Single Play")) { /* 클릭됨 */ }
//
// 키보드 병행:
//   - 위젯 자체는 마우스 전담이다. 메뉴 상하 커서 이동과 Enter 는 호출부에서
//     별도 처리 (gui 는 그 키를 해석하지 않는다).
//   - "강조된 항목" 을 기호로 그려야 할 땐 gui_button_highlighted 를 쓴다.

#include "../platform/platform.h"

// 마우스 포인터가 (x,y,w,h) 박스 안에 있는가.
bool gui_hover_rect(int x, int y, int w, int h);

// 사각형 버튼. 클릭되면 true (mouse 좌클릭 pressed 엣지).
//   - hover 시 밝게, pressed 시 더 밝게.
//   - 키보드로는 반응 안 함 (호출부가 별도 처리).
bool gui_button(int x, int y, int w, int h, const char* label, int fontSize = 24);

// 변형: 현재 메뉴 커서가 가리키는 항목처럼 강조 색으로 그림. 여전히 클릭도 받는다.
bool gui_button_highlighted(int x, int y, int w, int h, const char* label,
                            bool highlighted, int fontSize = 24);

// 우상단 X 모양 아이콘 버튼. 인게임 "나가기" 버튼용.
bool gui_close_button(int x, int y, int size);

// 모달 배경(반투명 오버레이)을 전체 화면에 덮는다. 모달 창은 위에 겹쳐 그린다.
// 화면 크기는 main.cpp 가 안다 → 인자로 받음.
void gui_modal_dim(int screenW, int screenH);

// 텍스트 수평 중앙 정렬로 한 줄 그리기 헬퍼.
void gui_text_center(int centerX, int y, const char* text, int fontSize, Color c);
