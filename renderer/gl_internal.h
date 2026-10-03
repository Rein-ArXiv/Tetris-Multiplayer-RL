#pragma once
#include "../platform/platform.h"
#include "gl_api.h"

// 렌더러 내부 전용. text_gl.cpp / image_gl.cpp 가 배처에 사각형을 넣을 때 쓴다.
//
// 도형의 정점을 호출 순서대로 모아 호환되는 렌더링 상태끼리 제출한다.
// 호출 횟수는 비어 있지 않은 flush 경계의 수에 따라 달라진다.
// 실제 성능은 드라이버 제출·전송·정점/픽셀 처리 비용을 함께 측정한다.

// 축 정렬 사각형 하나를 큐에 넣는다. 좌표는 논리 픽셀, 좌상단 원점.
//   radius  — 0 이면 각진 사각형. 양수면 fragment 셰이더가 모서리를 깎는다.
//   channel — 0.0 이면 RGBA 텍스처, 1.0 이면 R8 의 r 채널을 알파로 해석.
// view offset 은 이 함수 안에서 더해진다. 호출자는 논리 좌표만 넘긴다.
void glb_rect(GLuint tex,
              float x, float y, float w, float h,
              float u0, float v0, float u1, float v1,
              Color c, float radius, float channel);

// 회전된 사각형. 네 꼭짓점을 직접 준다 (TL, TR, BR, BL 순).
// 모서리 둥글리기는 지원하지 않는다 — 회전 이미지에는 쓰이지 않는다.
void glb_quad(GLuint tex,
              const float px[4], const float py[4],
              const float uu[4], const float vv[4],
              Color c, float channel);

// 큐를 GPU 명령으로 제출한다. 텍스처 변경·프레임 끝·아틀라스 재활용 전에 호출한다.
// GPU 완료나 모니터 표시 완료를 기다리는 함수는 아니다.
void glb_flush();

// Before deleting a texture, submit its pending vertices and forget its batch name.
// Main render thread/current context; this submits commands, not a GPU wait.
void glb_before_texture_delete(GLuint tex);

// 단색 도형용 1x1 흰색 텍스처. 셰이더를 하나로 유지하기 위한 장치다.
GLuint glb_white_texture();

// 현재 논리 해상도. 텍스트/이미지 쪽이 화면 밖 조기 반환에 쓴다.
int glb_screen_width();
int glb_screen_height();

// 논리 픽셀 하나가 실제 화면에서 몇 픽셀인가. 뷰포트 높이 / 논리 높이.
//
// 도형은 이 값과 무관하게 선명하다 — 정점 좌표가 실수라 GPU 가 뷰포트
// 해상도 그대로 래스터화한다. 문제는 글자다. 글리프는 CPU 에서 특정 픽셀
// 크기로 한 번 구워지므로, 논리 크기로 구워 놓고 4K 로 늘리면 그 배율만큼
// 흐려진다. text_gl.cpp 가 이 값을 곱해 실제 표시 크기로 굽는다.
float glb_render_scale();

// 폰트 서브시스템 정리 (text_gl.cpp 가 구현).
void renderer_text_shutdown();
