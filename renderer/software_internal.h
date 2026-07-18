#pragma once

#include <cstdint>
#include "../platform/platform.h"

// 소프트웨어 렌더러 서브시스템(text/image)이 공유하는 최소 픽셀 API.
// 좌표에는 renderer_set_view_offset 값이 자동으로 적용된다.
void software_blend_pixel(int x, int y, Color color);
void software_blend_coverage(int x, int y, Color color, uint8_t coverage);

int software_surface_width();
int software_surface_height();
