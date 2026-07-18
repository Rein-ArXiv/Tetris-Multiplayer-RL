#pragma once
#include "../platform/platform.h"

// ─────────────────────────────────────────────────────────────────────────────
// renderer/renderer.h — CPU 2D 소프트웨어 렌더러 인터페이스
//
// raylib의 BeginDrawing / DrawRectangle / DrawTextEx 등을 대체합니다.
// 구현: renderer/renderer.cpp
// ─────────────────────────────────────────────────────────────────────────────

// 고정 논리 해상도의 ARGB32 프레임버퍼와 이미지/폰트 서브시스템 생성.
// platform_init() 이후 한 번만 호출.
void renderer_init(int screen_w, int screen_h);

// 프레임 시작: CPU 프레임버퍼를 배경색으로 채운다.
// raylib::BeginDrawing() + ClearBackground() 대체.
void renderer_begin(Color bg);

// Section I — 전체 뷰를 (dx, dy) 픽셀만큼 시프트. 화면 흔들림에 사용.
// 이 호출 이후의 draw_rect/draw_text 가 전부 오프셋됨. 프레임 끝에 (0,0) 리셋 권장.
void renderer_set_view_offset(int dx, int dy);

// 프레임 종료: 완성된 CPU 프레임버퍼를 플랫폼 표시 계층에 전달.
void renderer_end();

// 렌더러 리소스 해제: CPU 픽셀, 이미지, 글리프 캐시.
// platform_shutdown() 전에 호출.
void renderer_shutdown();

// 폰트 로드. stb_truetype가 TTF를 파싱하고 글리프를 CPU 비트맵으로 캐시.
// path: "Font/monogram.ttf" 등 TTF 파일 경로
void renderer_load_font(const char* path);

// ─── 그리기 함수 ──────────────────────────────────────────────────────────────

// 색칠된 사각형. DrawRectangle() 대체.
// 클리핑 후 ARGB32 픽셀을 직접 채우며 알파 블렌딩한다.
void draw_rect(int x, int y, int w, int h, Color c);

// 둥근 모서리 사각형. DrawRectangleRounded() 대체.
// roundness: 0.0(직각) ~ 1.0(완전 둥근). 반지름 = roundness * min(w,h)/2.
void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c);

// 텍스트 그리기. DrawTextEx() / DrawText() 대체.
// stb_truetype로 만든 8-bit coverage mask를 CPU 알파 블렌딩한다.
void draw_text(const char* text, int x, int y, int size, Color c);

// 텍스트 폭 측정. MeasureTextEx() 대체.
// TTF advance metric으로 측정.
int  measure_text(const char* text, int size);
