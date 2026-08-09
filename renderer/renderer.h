#pragma once
#include "../platform/platform.h"

// ─────────────────────────────────────────────────────────────────────────────
// renderer/renderer.h — OpenGL 3.3 Core 2D 렌더러 인터페이스
//
// raylib의 BeginDrawing / DrawRectangle / DrawTextEx 등을 대체합니다.
// 구현: renderer/renderer.cpp (배처), text_gl.cpp (글자), image_gl.cpp (이미지)
// ─────────────────────────────────────────────────────────────────────────────

// 논리 해상도를 정하고 셰이더·정점 버퍼·이미지/폰트 서브시스템을 만든다.
// GL 컨텍스트가 필요하므로 반드시 platform_init() 이후 한 번만 호출.
void renderer_init(int screen_w, int screen_h);

// 프레임 시작: 뷰포트를 창에 맞추고 배경색으로 지운다.
// raylib::BeginDrawing() + ClearBackground() 대체.
void renderer_begin(Color bg);

// Section I — 전체 뷰를 (dx, dy) 픽셀만큼 시프트. 화면 흔들림에 사용.
// 이 호출 이후의 draw_rect/draw_text 가 전부 오프셋됨. 프레임 끝에 (0,0) 리셋 권장.
void renderer_set_view_offset(int dx, int dy);

// 프레임 종료: 남은 정점을 마저 그리고 버퍼를 교체한다.
void renderer_end();

// 렌더러 리소스 해제: GL 배치 자원, 이미지 텍스처, 글리프 아틀라스/캐시.
// platform_shutdown() 전에 호출.
void renderer_shutdown();

// 폰트 로드. stb_truetype가 TTF를 파싱하고, 필요한 글리프는 그릴 때
// CPU에서 래스터화한 뒤 GPU R8 아틀라스에 올린다.
// path: "Font/monogram.ttf" 등 TTF 파일 경로
void renderer_load_font(const char* path);

// ─── 그리기 함수 ──────────────────────────────────────────────────────────────

// 색칠된 사각형. DrawRectangle() 대체.
// 흰 텍스처를 쓰는 쿼드 정점을 배치에 추가하고, 알파 블렌딩은 GPU가 한다.
void draw_rect(int x, int y, int w, int h, Color c);

// 둥근 모서리 사각형. DrawRectangleRounded() 대체.
// roundness: 0.0(직각) ~ 1.0(완전 둥근). 반지름 = roundness * min(w,h)/2.
void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c);

// 텍스트 그리기. DrawTextEx() / DrawText() 대체.
// stb_truetype의 8-bit coverage 글리프를 아틀라스에서 샘플링하는 쿼드를
// 배치에 추가한다. 글자 모양은 CPU가 굽고 합성은 GPU가 맡는다.
void draw_text(const char* text, int x, int y, int size, Color c);

// 텍스트 폭 측정. MeasureTextEx() 대체.
// TTF advance metric으로 측정.
int  measure_text(const char* text, int size);
