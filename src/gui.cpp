#include "gui.h"
#include "../renderer/renderer.h"
#include <cstdint>
#include <limits>

static_assert(std::numeric_limits<int>::digits <= 31, "UI coordinates require at most 32-bit int");

namespace {
// 팔레트 — 메뉴/모달 전용. 기존 Color 상수(WHITE/GRAY 등)와 섞어 씀.
constexpr Color kBtnIdleBg    = { 38,  50,  78, 255};   // 어두운 남색
constexpr Color kBtnHoverBg   = { 60,  82, 140, 255};   // 호버 시 파랑
constexpr Color kBtnPressBg   = { 30,  60, 120, 255};   // 누른 채 포인터가 올라와 있는 상태
constexpr Color kBtnHighlight = {210, 180,  30, 255};   // 커서 강조 (키보드 선택)
constexpr Color kModalBg      = {  0,   0,   0, 180};   // 모달 오버레이 반투명
constexpr Color kCloseIdle    = {130, 130, 130, 255};
constexpr Color kCloseHover   = {230,  60,  60, 255};
}

// Widget layout uses int coordinates at the renderer boundary. Compute derived
// positions in a wider type, then narrow only after the whole layout is valid.
static bool fits_ui_coordinate(std::int64_t value)
{
    return value >= (std::numeric_limits<int>::min)() &&
           value <= (std::numeric_limits<int>::max)();
}

bool gui_hover_rect(int x, int y, int w, int h)
{
    int mx = platform_mouse_x();
    int my = platform_mouse_y();
    if (w <= 0 || h <= 0) return false;
    // Widen before addition: the far edge can lie outside the int domain.
    const auto right = std::int64_t(x) + w;
    const auto bottom = std::int64_t(y) + h;
    return mx >= x && std::int64_t(mx) < right &&
           my >= y && std::int64_t(my) < bottom;
}

bool gui_button(int x, int y, int w, int h, const char* label, int fontSize)
{
    const bool hover = gui_hover_rect(x, y, w, h);
    const bool press = hover && platform_mouse_down(0);
    Color bg = kBtnIdleBg;
    if (press)      bg = kBtnPressBg;
    else if (hover) bg = kBtnHoverBg;

    draw_rect_rounded(x, y, w, h, 0.25f, bg);
    // 라벨은 박스 중앙. measure_text 로 실 너비 측정해 가로 중앙 정렬.
    const int tw = measure_text(label, fontSize);
    const int tx = x + (w - tw) / 2;
    const int ty = y + (h - fontSize) / 2;
    draw_text(label, tx, ty, fontSize, WHITE);

    // 클릭 = hover 중 좌버튼 pressed 엣지.
    return hover && platform_mouse_pressed(0);
}

bool gui_button_highlighted(int x, int y, int w, int h, const char* label,
                            bool highlighted, int fontSize)
{
    const bool hover = gui_hover_rect(x, y, w, h);
    const bool press = hover && platform_mouse_down(0);
    Color bg;
    if (press)          bg = kBtnPressBg;
    else if (hover)     bg = kBtnHoverBg;
    else if (highlighted) bg = kBtnHighlight;
    else                bg = kBtnIdleBg;

    draw_rect_rounded(x, y, w, h, 0.25f, bg);
    const int tw = measure_text(label, fontSize);
    const int tx = x + (w - tw) / 2;
    const int ty = y + (h - fontSize) / 2;
    draw_text(label, tx, ty, fontSize, WHITE);
    return hover && platform_mouse_pressed(0);
}

bool gui_close_button(int x, int y, int size)
{
    const bool hover = gui_hover_rect(x, y, size, size);
    Color c = hover ? kCloseHover : kCloseIdle;
    // 배경 없이 X 선 두 개. 두께 3px, 내부 여백 size/4.
    const int pad = size / 4;
    const int th  = 3;
    // \ 대각선
    for (int i = 0; i < size - 2 * pad; ++i) {
        draw_rect(x + pad + i, y + pad + i, th, th, c);
    }
    // / 대각선
    for (int i = 0; i < size - 2 * pad; ++i) {
        draw_rect(x + size - pad - 1 - i, y + pad + i, th, th, c);
    }
    return hover && platform_mouse_pressed(0);
}

bool gui_checkbox(int x, int y, int size, const char* label, bool checked,
                  bool highlighted)
{
    // 라벨 폰트는 박스 높이에 맞춰 그린다. hover 영역은 박스 + 라벨 전체.
    if (size < 4) return false; // Two-pixel borders need a nonnegative interior.
    const int fontSize = size;
    const int gap = 10;
    const int tw = measure_text(label, fontSize);
    const auto hit_width = std::int64_t(size) + gap + tw;
    if (tw < 0 || !fits_ui_coordinate(hit_width) ||
        !fits_ui_coordinate(std::int64_t(x) + hit_width) ||
        !fits_ui_coordinate(std::int64_t(y) + size)) return false;
    const int hitW = static_cast<int>(hit_width);
    const bool hover = gui_hover_rect(x, y, hitW, size);

    // 박스 외곽선 — hover/highlight 시 강조색, 평소 회색.
    Color border;
    if (hover)            border = kBtnHoverBg;
    else if (highlighted) border = kBtnHighlight;
    else                  border = {120, 130, 170, 255};
    const int th = 2;
    draw_rect(x, y, size, th, border);                 // 상
    draw_rect(x, y + size - th, size, th, border);     // 하
    draw_rect(x, y, th, size, border);                 // 좌
    draw_rect(x + size - th, y, th, size, border);     // 우

    // 채워진 상태면 안쪽 사각형으로 체크 표시.
    if (checked) {
        const int pad = size / 4;
        draw_rect(x + pad, y + pad, size - 2 * pad, size - 2 * pad, border);
    }

    // 라벨 — 박스 오른쪽, 세로 중앙 정렬.
    const Color labelColor = highlighted ? kBtnHighlight : WHITE;
    draw_text(label, x + size + gap, y, fontSize, labelColor);

    return hover && platform_mouse_pressed(0);
}

int gui_slider(int x, int y, int w, int h, int valuePct, bool highlighted)
{
    if (valuePct < 0)   valuePct = 0;
    if (valuePct > 100) valuePct = 100;

    const bool hover = gui_hover_rect(x, y, w, h);

    // 트랙 — 가는 가로 바 (세로 중앙). 채워진 구간은 강조색.
    const int trackH = 6;
    const int trackY = y + (h - trackH) / 2;
    Color trackBg   = {60, 66, 96, 255};
    Color fillColor = highlighted ? kBtnHighlight : kBtnHoverBg;
    const int fillW = w * valuePct / 100;
    draw_rect(x, trackY, w, trackH, trackBg);
    draw_rect(x, trackY, fillW, trackH, fillColor);

    // 노브 — 채워진 구간 끝의 작은 사각형. 양 끝(0%/100%)에서 트랙 밖으로
    // 삐져나가지 않도록 [x, x+w-knobW] 로 clamp 한다.
    const int knobW = 10;
    int knobX = x + fillW - knobW / 2;
    if (knobX < x)             knobX = x;
    if (knobX > x + w - knobW) knobX = x + w - knobW;
    const Color knob = (hover || highlighted) ? WHITE : Color{200, 205, 225, 255};
    draw_rect(knobX, y + h / 2 - knobW, knobW, knobW * 2, knob);

    // 드래그/클릭 — 트랙 위에서 좌버튼이 눌려있으면 그 x 위치로 값을 설정.
    if (hover && platform_mouse_down(0)) {
        int mx = platform_mouse_x();
        int v  = (w > 0) ? (mx - x) * 100 / w : valuePct;
        if (v < 0)   v = 0;
        if (v > 100) v = 100;
        return v;
    }
    return valuePct;
}

int gui_value_selector(int x, int y, int w, int h, const char* label,
                       bool highlighted, bool allowPrevious, bool allowNext)
{
    // Keep two square arrows disjoint with a positive center label interval.
    if (h <= 6 || std::int64_t(w) <= 2 * std::int64_t(h) ||
        !fits_ui_coordinate(std::int64_t(x) + w) ||
        !fits_ui_coordinate(std::int64_t(y) + h)) return 0;
    const int arrowW = h;
    const int fs = h - 6;
    const int left_width = measure_text("<", fs);
    const int right_width = measure_text(">", fs);
    const int text_width = measure_text(label, fs);
    if (left_width < 0 || right_width < 0 || text_width < 0) return 0;
    const auto right_x = std::int64_t(x) + w - arrowW;
    const auto left_text_x = std::int64_t(x) + (std::int64_t(arrowW) - left_width) / 2;
    const auto right_text_x = right_x + (std::int64_t(arrowW) - right_width) / 2;
    const auto center_text_x = std::int64_t(x) + (std::int64_t(w) - text_width) / 2;
    if (!fits_ui_coordinate(left_text_x) || !fits_ui_coordinate(right_text_x) ||
        !fits_ui_coordinate(center_text_x)) return 0;

    const Color arrowIdle = highlighted ? kBtnHighlight : Color{180, 190, 220, 255};
    const bool hoverL = allowPrevious && gui_hover_rect(x, y, arrowW, h);
    const bool hoverR = allowNext && gui_hover_rect(static_cast<int>(right_x), y, arrowW, h);
    const Color cL = hoverL ? WHITE : arrowIdle;
    const Color cR = hoverR ? WHITE : arrowIdle;
    if (allowPrevious) draw_text("<", static_cast<int>(left_text_x), y + 3, fs, cL);
    if (allowNext) draw_text(">", static_cast<int>(right_text_x), y + 3, fs, cR);
    const Color labelColor = highlighted ? kBtnHighlight : WHITE;
    draw_text(label, static_cast<int>(center_text_x), y + 3, fs, labelColor);

    if (hoverL && platform_mouse_pressed(0)) return -1;
    if (hoverR && platform_mouse_pressed(0)) return +1;
    return 0;
}

void gui_modal_dim(int screenW, int screenH)
{
    draw_rect(0, 0, screenW, screenH, kModalBg);
}

void gui_text_center(int centerX, int y, const char* text, int fontSize, Color c)
{
    const int tw = measure_text(text, fontSize);
    draw_text(text, centerX - tw / 2, y, fontSize, c);
}
