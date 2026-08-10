#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// platform/platform.h  — OS 추상화 인터페이스
//
// 기성 즉시 그리기 라이브러리의 창·입력·시간 API 를 대체한다.
// 구현은 platform/win32.cpp에 있습니다.
//
// 학습 포인트:
//   "창을 하나 연다" 한 줄은 아래 platform_init() 이 호출하는 80줄을 숨겨놓은 것.
//   "이 키가 눌렸는가" 조회는 WM_KEYDOWN 메시지로 채우는 keyState[] 테이블 조회.
//   프레임 델타타임은 QueryPerformanceCounter 두 번의 차이.
// ─────────────────────────────────────────────────────────────────────────────

// ─── 색상 ─────────────────────────────────────────────────────────────────────
// 이전에 쓰던 즉시 그리기 라이브러리의 색 구조체 { r, g, b, a } 와 동일한 레이아웃.
struct Color { uint8_t r, g, b, a; };

// 공통 색상 상수 (main.cpp 변경을 최소화하기 위해 이전 라이브러리의 이름 유지)
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

// 윈도우와 입력/타이머 백엔드 초기화. OpenGL 3.3 Core 컨텍스트를 함께 만든다.
// 컨텍스트 생성에 실패하면 프로그램을 계속 진행할 수 없으므로 즉시 실패한다.
// 실패 시 platform_should_close() 가 true 가 되므로 호출자는 반드시 확인한다.
void   platform_init(int w, int h, const char* title);

// 창을 띄우지 못했거나 렌더러를 만들지 못했을 때 사용자에게 이유를 보여준다.
// GUI 프로그램은 stderr 가 보이지 않는다 — 콘솔 없이 실행하면 진단 메시지가
// 그대로 사라져 사용자에게는 "검은 창" 또는 "아무 일도 안 일어남" 만 남는다.
// Windows 는 MessageBox, SDL 은 SDL_ShowSimpleMessageBox 로 띄운다.
void   platform_fatal_error(const char* message);

// 윈도우 및 플랫폼 자원 해제.
void   platform_shutdown();

// 창 닫기 요청(WM_CLOSE / WM_DESTROY / SDL_QUIT)을 받으면 true.
// ESC 키는 여기 관여하지 않는다 — 화면별 뒤로가기로만 쓰인다.
bool   platform_should_close();

// 프레임 시작: 이전 키 상태 스냅샷 + 메시지 루프(PeekMessage) + 델타타임 반환.
// MAX_DELTA = 100ms 클램핑 포함.
float  platform_begin_frame();

// 프레임 끝. 소프트웨어 VSync가 켜졌다면 60 Hz에 맞춰 남은 시간을 쉰다.
void   platform_end_frame();

// 그린 프레임을 화면에 내보낸다 (버퍼 교체).
void   platform_present();

// ─── OpenGL 연동 ─────────────────────────────────────────────────────────────
// GL 함수 포인터 조회. Windows 의 opengl32.dll 은 GL 1.1 만 export 하므로
// 3.3 Core 의 거의 모든 함수는 런타임에 이 경로로 받아야 한다.
// 렌더러가 gl_load_functions() 에서 이 함수를 반복 호출한다.
void*  platform_gl_get_proc(const char* name);

// 논리 화면이 실제로 그려질 창 안의 사각형(픽셀). 창 리사이즈를 따라간다.
// 렌더러가 glViewport 에 그대로 넘기는 값이라 GL 규약대로 좌하단 원점이다.
//
// 창 종횡비가 논리 종횡비와 다르면 여기서 레터박스가 결정된다. 마우스 좌표를
// 논리 좌표로 되돌리는 계산도 같은 사각형을 쓰므로, 이 둘이 어긋나면 클릭
// 지점과 그려진 버튼이 서로 다른 곳을 가리키게 된다.
void   platform_viewport(int& x_out, int& y_out, int& w_out, int& h_out);

// 이 프레임에 처음 눌린 키인가? (edge)
// keyState[key] == true && keyPrev[key] == false
bool   platform_key_pressed(int key);

// 현재 눌려있는 키인가? (level)
bool   platform_key_down(int key);

// WM_CHAR 로 받은 문자 하나 꺼내기 (없으면 0).
char   platform_get_char_pressed();

// ─── 마우스 ───────────────────────────────────────────────────────────────────
// 버튼 인덱스: 0 = Left, 1 = Right, 2 = Middle.
// 좌표는 클라이언트 영역 기준 (0,0 = 좌상단). 창 밖이면 마지막 값 유지.
int    platform_mouse_x();
int    platform_mouse_y();
// 이번 프레임에 처음 눌림 (edge).
bool   platform_mouse_pressed(int button);
// 현재 누르고 있음 (level).
bool   platform_mouse_down(int button);
// 이번 프레임에 뗌 (edge).
bool   platform_mouse_released(int button);
// 이번 프레임 휠 스크롤 누적 (위로 양수). 없으면 0.
float  platform_mouse_wheel();

// platform_init 이후 경과 초.
double platform_get_time();

// ─── 윈도우 설정 (렌더/UI 전용 — SimGame/결정성과 무관) ──────────────────────────
// 창 크기를 (w,h) 로 바꾸고 화면 중앙에 재배치. 표시 영역을 갱신.
// GUI 는 platform_init 에 넘긴 논리 크기(720×640)를 기준 좌표로 쓰므로,
// 마우스 좌표는 항상 논리 좌표로 역매핑된다 (아래 platform_mouse_x/y 참고).
void   platform_set_window_size(int w, int h);

// 창을 놓을 수 있는 화면 영역(픽셀). 작업 표시줄 등을 뺀 크기다.
// 설정 화면이 모니터보다 큰 창 프리셋을 감추는 데 쓴다 — 2430x2160 을
// 1920x1080 모니터에서 고르면 창의 절반이 화면 밖으로 나간다.
void   platform_display_size(int& w_out, int& h_out);

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
