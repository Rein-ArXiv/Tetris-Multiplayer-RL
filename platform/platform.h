#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// platform/platform.h  — OS 추상화 인터페이스
//
// raylib의 InitWindow / IsKeyPressed / GetFrameTime 등을 대체합니다.
// 구현은 platform/win32.cpp에 있습니다.
//
// 학습 포인트:
//   raylib::InitWindow() 는 아래 platform_init() 이 호출하는 80줄을 숨겨놓은 것.
//   raylib::IsKeyPressed()는 WM_KEYDOWN 메시지로 채우는 keyState[] 테이블 조회.
//   raylib::GetFrameTime()은 QueryPerformanceCounter 두 번의 차이.
// ─────────────────────────────────────────────────────────────────────────────

// ─── 색상 ─────────────────────────────────────────────────────────────────────
// raylib의 Color { r, g, b, a } 와 동일한 레이아웃.
struct Color { uint8_t r, g, b, a; };

// 공통 색상 상수 (main.cpp 변경을 최소화하기 위해 raylib 이름 유지)
constexpr Color WHITE    = {255, 255, 255, 255};
constexpr Color GRAY     = {130, 130, 130, 255};
constexpr Color GREEN    = {0,   228,  48, 255};
constexpr Color YELLOW   = {253, 249,   0, 255};
constexpr Color RED      = {230,  41,  55, 255};
constexpr Color RAYWHITE = {245, 245, 245, 255};

// ─── 키코드 ───────────────────────────────────────────────────────────────────
// 값이 Win32 VK_* 상수와 직접 대응하므로 별도 매핑 테이블이 필요 없습니다.
// WndProc의 WM_KEYDOWN 에서 wParam 을 그대로 keyState[] 인덱스로 씁니다.
enum PlatformKey : int {
    PKEY_LEFT   = 0x25,  // VK_LEFT
    PKEY_RIGHT  = 0x27,  // VK_RIGHT
    PKEY_UP     = 0x26,  // VK_UP
    PKEY_DOWN   = 0x28,  // VK_DOWN
    PKEY_SPACE  = 0x20,  // VK_SPACE
    PKEY_ENTER  = 0x0D,  // VK_RETURN
    PKEY_ESCAPE = 0x1B,  // VK_ESCAPE
    PKEY_BACK   = 0x08,  // VK_BACK (Backspace)
    PKEY_Q      = 'Q',
    PKEY_R      = 'R',
    PKEY_H      = 'H',
    PKEY_P      = 'P',
    PKEY_C      = 'C',
    PKEY_J      = 'J',
    PKEY_T      = 'T',
    PKEY_Y      = 'Y',
    PKEY_N      = 'N',
    PKEY_LBRACKET = 0xDB,  // VK_OEM_4: [
    PKEY_RBRACKET = 0xDD,  // VK_OEM_6: ]
    PKEY_F5     = 0x74,  // VK_F5
    PKEY_F6     = 0x75,  // VK_F6
};

// ─── 플랫폼 API ───────────────────────────────────────────────────────────────

// 윈도우와 입력/타이머 백엔드 초기화. 그래픽 API 컨텍스트는 만들지 않는다.
void   platform_init(int w, int h, const char* title);

// 윈도우 및 플랫폼 자원 해제. CloseWindow() 대체.
void   platform_shutdown();

// WM_QUIT 또는 ESC 키를 받으면 true 반환.
bool   platform_should_close();

// 프레임 시작: 이전 키 상태 스냅샷 + 메시지 루프(PeekMessage) + 델타타임 반환.
// GetFrameTime() 대체. MAX_DELTA = 100ms 클램핑 포함.
float  platform_begin_frame();

// 프레임 끝. 소프트웨어 VSync가 켜졌다면 60 Hz에 맞춰 남은 시간을 쉰다.
void   platform_end_frame();

// CPU ARGB32 프레임버퍼를 창에 표시한다. pixels의 각 uint32_t는
// 0xAARRGGBB이며, pitch_bytes는 한 행의 바이트 수다.
void   platform_present(const uint32_t* pixels, int w, int h, int pitch_bytes);

// 이 프레임에 처음 눌린 키인가? IsKeyPressed() 대체.
// keyState[key] == true && keyPrev[key] == false
bool   platform_key_pressed(int key);

// 현재 눌려있는 키인가? IsKeyDown() 대체.
bool   platform_key_down(int key);

// WM_CHAR 로 받은 문자 하나 꺼내기 (없으면 0). GetCharPressed() 대체.
char   platform_get_char_pressed();

// ─── 마우스 ───────────────────────────────────────────────────────────────────
// 버튼 인덱스: 0 = Left, 1 = Right, 2 = Middle.
// 좌표는 클라이언트 영역 기준 (0,0 = 좌상단). 창 밖이면 마지막 값 유지.
int    platform_mouse_x();
int    platform_mouse_y();
// 이번 프레임에 처음 눌림 (edge). IsMouseButtonPressed 대체.
bool   platform_mouse_pressed(int button);
// 현재 누르고 있음 (level). IsMouseButtonDown 대체.
bool   platform_mouse_down(int button);
// 이번 프레임에 뗌 (edge). IsMouseButtonReleased 대체.
bool   platform_mouse_released(int button);
// 이번 프레임 휠 스크롤 누적 (위로 양수). 없으면 0. GetMouseWheelMove 대체.
float  platform_mouse_wheel();

// platform_init 이후 경과 초. GetTime() 대체.
double platform_get_time();

// ─── 윈도우 설정 (렌더/UI 전용 — SimGame/결정성과 무관) ──────────────────────────
// 창 크기를 (w,h) 로 바꾸고 화면 중앙에 재배치. 표시 영역을 갱신.
// GUI 는 platform_init 에 넘긴 논리 크기(720×640)를 기준 좌표로 쓰므로,
// 마우스 좌표는 항상 논리 좌표로 역매핑된다 (아래 platform_mouse_x/y 참고).
void   platform_set_window_size(int w, int h);

// 전체화면 토글. on=true 면 데스크톱-해상도 전체화면(FULLSCREEN_DESKTOP),
// off 면 창 모드로 복귀. 전체화면에서 모니터 종횡비가 논리(9:8) 와 다르면
// 9:8 을 유지하는 레터박스 뷰포트로 그려 왜곡을 막는다.
void   platform_set_fullscreen(bool on);

// 이 백엔드가 전체화면을 실제로 지원하는가. SDL 백엔드는 true,
// Win32 백엔드는 미구현이라 false. 설정 화면이 false 면 Fullscreen 행을
// 비활성(회색) 으로 그려 "켜도 아무 일 없는" 거짓 토글을 막는다.
bool   platform_fullscreen_supported();

// 소프트웨어 프레임 페이싱 on/off. on이면 platform_end_frame이 60 Hz를 목표로 한다.
void   platform_set_vsync(bool on);
