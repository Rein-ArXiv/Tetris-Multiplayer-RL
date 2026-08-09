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
//
// 실패(GL 3.3 진입점 누락 / 셰이더 컴파일·링크 실패)하면 false 를 돌려준다.
// 이때 이후의 draw_* 는 전부 무시되므로 게임은 화면 없이 돌아간다 — 호출자는
// 반드시 반환값을 확인하고 사용자에게 이유를 알린 뒤 종료해야 한다.
bool renderer_init(int screen_w, int screen_h);

// 프레임 시작: 뷰포트를 창에 맞추고 배경색으로 지운다.
// raylib::BeginDrawing() + ClearBackground() 대체.
void renderer_begin(Color bg);

// Section I — 전체 뷰를 (dx, dy) 픽셀만큼 시프트. 화면 흔들림에 사용.
// 이 호출 이후의 draw_rect/draw_text 가 전부 오프셋됨. 프레임 끝에 (0,0) 리셋 권장.
void renderer_set_view_offset(int dx, int dy);

// 프레임 종료: 남은 정점을 마저 그리고 버퍼를 교체한다.
void renderer_end();

// GL 자원 해제: 셰이더 프로그램, 정점 버퍼, 이미지 텍스처, 글리프 아틀라스.
// 컨텍스트가 살아 있을 때만 유효하므로 반드시 platform_shutdown() 전에 호출.
void renderer_shutdown();

// 폰트 로드. stb_truetype 가 TTF 를 파싱한다. 필요한 글리프는 처음 그릴 때
// CPU 에서 래스터화되어 GPU 글리프 아틀라스(R8 텍스처)에 올라간다.
// path: "Font/NanumGothic.ttf" 등 TTF 파일 경로
void renderer_load_font(const char* path);

// ─── 그리기 함수 ──────────────────────────────────────────────────────────────
//
// 아래 함수들은 즉시 그리지 않는다. 정점을 배처에 쌓아 두고, 텍스처가 바뀌는
// 지점과 프레임 끝(renderer_end)에서만 실제 draw call 이 나간다.

// 색칠된 사각형. DrawRectangle() 대체.
// 1x1 흰 텍스처를 입힌 쿼드 두 삼각형으로 배처에 들어가고, 알파 블렌딩은
// GPU 가 한다.
void draw_rect(int x, int y, int w, int h, Color c);

// 둥근 모서리 사각형. DrawRectangleRounded() 대체.
// roundness: 0.0(직각) ~ 1.0(완전 둥근). 반지름 = roundness * min(w,h)/2.
// 모서리는 fragment 셰이더가 SDF 로 깎으므로 안티앨리어싱이 함께 적용된다.
void draw_rect_rounded(int x, int y, int w, int h, float roundness, Color c);

// 텍스트 그리기. DrawTextEx() / DrawText() 대체.
// 글리프는 아틀라스의 R8 텍셀이며, 셰이더가 r 채널을 알파로 읽어 색을 곱한다.
// 글자 모양은 CPU 가 굽고 합성은 GPU 가 맡는다. 배치는 논리 좌표로 하되
// 비트맵은 화면 배율로 구워 확대해도 선명하다.
void draw_text(const char* text, int x, int y, int size, Color c);

// 텍스트 폭 측정. MeasureTextEx() 대체.
// TTF advance metric 으로 측정한다. 창 배율과 무관한 논리 픽셀 값이라
// 창을 늘려도 레이아웃이 흔들리지 않는다.
int  measure_text(const char* text, int size);
