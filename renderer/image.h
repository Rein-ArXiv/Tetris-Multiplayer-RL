#pragma once
#include <cstdint>
#include "renderer.h"

// ─────────────────────────────────────────────────────────────────────────────
// renderer/image.h — PNG/JPG 아이콘 로더 + 텍스처 쿼드 드로우
//
// 사용 예:
//   ImageHandle h = image_load("assets/icons/player.png");
//   draw_image(h, 100, 200, 64, 64);          // 원본 색
//   draw_image_tinted(h, 100, 200, 64, 64, RED);
//   image_unload(h);
//
// 학습 포인트:
//   load : 디코더(Win32=GDI+)가 RGBA 8bpp 픽셀 배열을 내놓으면
//          glGenTextures → glTexImage2D 로 GPU 로 업로드.
//   draw : kSprite* 셰이더 + a_pos(xy) + a_uv(UV) 두 vertex attribute 인 VAO 로
//          6 vertex 쿼드 한 번 렌더.
//   알파 블렌딩은 draw 진입 시 자동 활성화 (draw_rect 계열에 영향 없음).
// ─────────────────────────────────────────────────────────────────────────────

using ImageHandle = int;  // 0 = invalid/미로드

// 실패 시 0 리턴 (파일 없음, 디코드 실패, GL 초기화 전 호출 등).
// 성공 시 양수 핸들.
ImageHandle image_load(const char* path);

// RGBA8 픽셀 배열에서 이미지 생성. 기본/절차적 fallback 아이콘 등에 사용.
// pixels 는 w*h*4 바이트이며 호출 시점에 GPU 로 복사된다.
ImageHandle image_create_rgba(const uint8_t* pixels, int w, int h);

// 해제. 핸들이 0 이거나 유효하지 않으면 no-op.
void image_unload(ImageHandle h);

// 픽셀 단위. (x, y) 는 좌상단. 좌상단이 텍스처 (0,0) 에 매핑.
void draw_image(ImageHandle h, int x, int y, int w, int h_px);

// tint 는 RGBA 각 채널에 곱해짐. {255,255,255,255} = 원본.
void draw_image_tinted(ImageHandle h, int x, int y, int w, int h_px, Color tint);

// 이미지 크기 질의 — 원본 너비/높이가 필요할 때 (예: 자연 크기로 드로우).
//   반환 false = 핸들 무효.
bool image_size(ImageHandle h, int& w_out, int& h_out);

// 내부: renderer_init 시점 호출 — 스프라이트 셰이더/VAO 일회성 초기화.
void image_init();
void image_shutdown();
