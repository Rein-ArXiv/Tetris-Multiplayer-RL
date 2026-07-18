#pragma once
#include <cstdint>
#include "renderer.h"

// ─────────────────────────────────────────────────────────────────────────────
// renderer/image.h — PNG/JPG 아이콘 로더 + CPU 스프라이트 드로우
//
// 사용 예:
//   ImageHandle h = image_load("assets/icons/player.png");
//   draw_image(h, 100, 200, 64, 64);          // 원본 색
//   draw_image_tinted(h, 100, 200, 64, 64, RED);
//   image_unload(h);
//
// 학습 포인트:
//   load : 디코더(Win32=GDI+, 그 외=stb_image)의 RGBA8를 CPU ARGB32로 변환.
//   draw : 목적지 픽셀에서 원본 좌표를 역산해 샘플링하고 직접 알파 블렌딩.
// ─────────────────────────────────────────────────────────────────────────────

using ImageHandle = int;  // 0 = invalid/미로드

// 실패 시 0 리턴 (파일 없음, 디코드 실패 등).
// 성공 시 양수 핸들.
ImageHandle image_load(const char* path);

// RGBA8 픽셀 배열에서 이미지 생성. 기본/절차적 fallback 아이콘 등에 사용.
// pixels 는 w*h*4 바이트이며 호출 시점에 CPU 이미지 저장소로 복사된다.
ImageHandle image_create_rgba(const uint8_t* pixels, int w, int h);

// 해제. 핸들이 0 이거나 유효하지 않으면 no-op.
void image_unload(ImageHandle h);

// 픽셀 단위. (x, y) 는 좌상단. 좌상단이 텍스처 (0,0) 에 매핑.
void draw_image(ImageHandle h, int x, int y, int w, int h_px);

// tint 는 RGBA 각 채널에 곱해짐. {255,255,255,255} = 원본.
void draw_image_tinted(ImageHandle h, int x, int y, int w, int h_px, Color tint);

// 회전 드로우 — (cx, cy) 가 중심, angle_deg 는 시계방향(화면 y 가 아래로
// 증가하므로 표준 수학 좌표계의 반시계와 반대). 쿼드 4꼭짓점을 CPU 에서
// 회전한 목적지 사각형에서 원본 좌표를 역변환한다. 메뉴/상점의 실시간
// 회전 아이콘용이다.
void draw_image_rotated(ImageHandle h, int cx, int cy, int w, int h_px,
                        float angle_deg);

// 이미지 크기 질의 — 원본 너비/높이가 필요할 때 (예: 자연 크기로 드로우).
//   반환 false = 핸들 무효.
bool image_size(ImageHandle h, int& w_out, int& h_out);

// 내부: renderer_init 시점 호출 — CPU 이미지 핸들 저장소 초기화.
void image_init();
void image_shutdown();
