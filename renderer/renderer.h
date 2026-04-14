#pragma once
#include "../platform/platform.h"

// ─────────────────────────────────────────────────────────────────────────────
// renderer/renderer.h — 2D OpenGL 렌더러 인터페이스
//
// raylib의 BeginDrawing / DrawRectangle / DrawTextEx 등을 대체합니다.
// 구현: renderer/renderer.cpp
// ─────────────────────────────────────────────────────────────────────────────

// 셰이더 컴파일, VAO/VBO 생성, 폰트 로드.
// platform_init() 이후 한 번만 호출.
void renderer_init(int screen_w, int screen_h);

// 프레임 시작: glClear + 투영 행렬 업로드.
// raylib::BeginDrawing() + ClearBackground() 대체.
void renderer_begin(Color bg);

// 프레임 종료 (현재는 빈 함수 — platform_end_frame이 SwapBuffers 담당).
// raylib::EndDrawing() 대체 (SwapBuffers는 platform_end_frame에서).
void renderer_end();

// 렌더러 리소스 해제: GL program, VAO, VBO, 폰트 디스플레이 리스트.
// platform_shutdown() 전에 호출.
void renderer_shutdown();

// 폰트 로드. 내부에서 wglUseFontBitmaps로 GDI 폰트를 GL 디스플레이 리스트로 변환.
// path: "Font/monogram.ttf" 등 TTF 파일 경로
void renderer_load_font(const char* path);

// ─── 그리기 함수 ──────────────────────────────────────────────────────────────

// 색칠된 사각형. DrawRectangle() 대체.
// 내부: 6개 꼭짓점(2개 삼각형) → VBO 업데이트 → glDrawArrays
void draw_rect(int x, int y, int w, int h, Color c);

// 둥근 모서리 사각형. DrawRectangleRounded() 대체.
// roundness: 0.0(직각) ~ 1.0(완전 둥근). 반지름 = roundness * min(w,h)/2.
void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c);

// 텍스트 그리기. DrawTextEx() / DrawText() 대체.
// wglUseFontBitmaps + glWindowPos2i + glCallLists 로 구현.
// size: 픽셀 단위 폰트 크기 (각 크기마다 GDI 폰트 리스트를 캐시)
void draw_text(const char* text, int x, int y, int size, Color c);

// 텍스트 폭 측정. MeasureTextEx() 대체.
// GetTextExtentPoint32 로 GDI에서 측정.
int  measure_text(const char* text, int size);
