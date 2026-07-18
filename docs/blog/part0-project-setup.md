# Part 0: 프로젝트 셋업 — 디렉토리·의존성·CMake

> **시리즈:** 제로부터 멀티플레이어 테트리스 + RL까지
> [시리즈 목차](./README.md) · **Part 0** · [다음: Part 1 — 결정론적 SimGame](./part1-deterministic-simulation.md)

---

## 이 장의 구현 계약

- **선행 상태:** 빈 저장소와 C++17 컴파일러만 전제한다.
- **이번 장의 파일:** `CMakeLists.txt`, 최상위 디렉터리, `third_party/` 경계.
- **연결점:** 이후 모든 Part가 같은 CMake 옵션과 타깃 이름을 확장한다.
- **완료 게이트:** 현재 저장소에서는 기본 configure가 성공하고, 필요한 의존성이
  없을 때 어느 옵션에서 실패하는지 설명할 수 있어야 한다.

## 들어가며

이미 존재하는 게임 엔진을 통해 시작하는 테트리스 튜토리얼은 어디에나 있다. 보통 이미 에디터가 켜져 있고, 거기서 사각형을 집어넣는 것으로 블록이 그려진다. 애셋을 넣고, 코드를 넣으면 창 · 입력 · 오디오 · 폰트 · 이미지 로더가 전부 따라온다. 그 튜토리얼에서 "CMake" 는 거의 보이지 않는다.

이 시리즈는 그 반대로 간다. Windows 에서는 Win32 API 로 창을 만들고, macOS/Linux 에서는 SDL2 에 창·입력·최종 화면 복사만 맡긴다. 도형·텍스트·이미지는 720×640 ARGB32 메모리에 직접 쓰는 자체 2D 소프트웨어 렌더러가 처리한다. 오디오는 XAudio2 또는 SDL audio subsystem, 폰트 래스터화는 단일 헤더 `stb_truetype`, 이미지 디코딩은 GDI+(Windows) 또는 단일 헤더 `stb_image`(비-Win32), MP3 디코딩은 단일 헤더 `dr_mp3`, 네트워킹은 WinSock2/BSD 소켓 순정, 봇 추론은 ONNX Runtime CPU 바이너리만 링크, Python RL 레이어는 pybind11 로 `SimGame` 을 노출한다. 이 모든 것이 하나의 `CMakeLists.txt` 에서 관리된다.

왜 이렇게 만드는가. 첫째, **결정론**. 상용 엔진 내부의 입력 큐잉·오디오 스케줄러가 블랙박스로 결과를 뒤흔들면 lockstep 멀티플레이에서 상태가 갈라진다. 각 레이어를 직접 소유해야 "같은 시드 + 같은 입력 = 같은 결과" 를 60Hz 로 보장할 수 있다. 둘째, **이식성의 최소 절단면**. 게임 실행 파일은 Windows/macOS/Linux, 릴레이 서버는 Termux(Android) 까지 간다. 외부 의존성을 줄일수록 "이 플랫폼에서는 이 옵션 꺼라" 가 단순해진다. 셋째, **교육적 가치**. 한 줄이 감추는 여섯 단계를 한 번쯤 직접 써보면 이후 어떤 엔진을 쓰더라도 오류 메시지를 해석할 수 있다.

이 장은 그 모든 재료를 꺼내놓고 빌드 시스템에 줄 세우는 과정을 다룬다.
`CMakeLists.txt`를 섹션별로 해부하고, 각 옵션이 어떤 소스와 라이브러리를
끌어오는지 추적한다. Part 1로 넘어가기 전에 각 타깃의 계층 경계와
`cmake -B build && cmake --build build`의 의미를 설명할 수 있어야 한다.

```mermaid
graph TB
    A[CMakeLists.txt<br/>build entry] --> B[tetris<br/>game client]
    A --> C[sim_hash_dump<br/>determinism test]
    A --> D[tetris_relay<br/>matchmaking relay]
    A --> E[tetris_meta<br/>HTTP + SQLite]
    A --> F[tetris_py<br/>pybind11]

    B --> G[src/ + core/ + net/ + renderer/ + platform/ + audio/]
    C --> H[src/sim_game.cpp + core/]
    D --> I[server/ + net/ + meta/http_client.cpp]
    E --> J[meta/ + third_party/sqlite3.c + httplib.h]
    F --> K[bindings/tetris_py.cpp + src/sim_game.cpp]
```

---

## 0. 빈 저장소의 첫 체크포인트

처음부터 최종 `CMakeLists.txt`를 복사하면 아직 만들지 않은 수십 개의 source가
없어 configure부터 실패한다. 먼저 실행 파일 하나만 가진 뼈대로 시작하고, 아래
§3의 최종 타깃 구조는 각 Part에서 조금씩 확장한다.

```cmake
# Part 0 체크포인트: CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(tetris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tetris src/main.cpp)
```

```cpp
// Part 0 체크포인트: src/main.cpp
#include <iostream>

int main()
{
    std::cout << "tetris project skeleton\n";
    return 0;
}
```

```bash
cmake -B build
cmake --build build
./build/tetris
```

Windows multi-config generator에서는 `build/Debug/tetris.exe`처럼 configuration
하위에 생성된다. 기대 출력은 `tetris project skeleton` 한 줄이다. 이 상태가
Part 1의 시작점이다.

---

## 1. 레포 구조 한눈에

저장소 최상위에서 `ls` 를 치면 다음이 보인다:

```text
Tetris-Multiplayer-RL/
├── CMakeLists.txt         ← 빌드 진입점
├── README.md              ← 완성 상태의 실행·아키텍처 요약
├── docs/blog/             ← Part 0~12 누적 구현 문서
│
├── core/                  ← 순수 유틸 (외부 의존 없음)
│   ├── constants.h        ← TICKS_PER_SECOND=60 등
│   ├── input.h            ← INPUT_LEFT/RIGHT/... 비트마스크
│   ├── rng.h              ← XorShift64* 결정론 RNG
│   ├── hash.h             ← FNV-1a 64-bit 상태 해시
│   └── replay.h/.cpp      ← 입력 리플레이 저장/로드
│
├── src/                   ← 게임 로직 + 렌더링 래퍼 + 진입점
│   ├── sim_game.h/.cpp    ← SimGame (헤드리스 시뮬)
│   ├── sim_grid.h         ← 20×10 보드
│   ├── sim_block.h        ← 테트로미노 상태
│   ├── sim_blocks.h       ← L/J/I/O/S/T/Z 팩토리
│   ├── game.h/.cpp        ← SimGame + 렌더링 래퍼
│   ├── colors.h/.cpp      ← 팔레트
│   ├── position.h/.cpp    ← (row, col)
│   └── main.cpp           ← 진입점, FSM, 60Hz 루프
│
├── platform/              ← 창/입력 백엔드 (둘 중 하나 선택)
│   ├── platform.h         ← 공용 인터페이스
│   ├── win32.cpp          ← Win32 + GDI 표시 (Windows 기본)
│   ├── sdl.cpp            ← SDL2 surface 표시 (macOS/Linux 기본)
│   └── macos/Info.plist.in
│
├── renderer/              ← CPU 2D 소프트웨어 렌더러
│   ├── renderer.h/.cpp    ← ARGB32 framebuffer, rect, alpha blend
│   ├── software_internal.h← 텍스트/이미지가 공유하는 픽셀 API
│   ├── text_software.cpp  ← stb_truetype CPU 글리프 캐시
│   ├── shake.h/.cpp       ← 화면 흔들림
│   └── image.h/.cpp       ← PNG decode, CPU sampling/tint/rotation
│
├── audio/                 ← 오디오 백엔드
│   ├── audio.h            ← 공용 인터페이스
│   ├── audio.cpp          ← XAudio2 (Win32 경로)
│   └── sdl_audio.cpp      ← SDL audio subsystem (SDL2 경로)
│
├── net/                   ← TCP 네트워킹 3계층
│   ├── socket.h/.cpp      ← 크로스플랫폼 TCP
│   ├── framing.h/.cpp     ← 메시지 직렬화
│   └── session.h/.cpp     ← lockstep P2P 세션
│
├── server/                ← tetris_relay (헤드리스)
│   ├── main.cpp
│   ├── player_conn.h/.cpp
│   ├── matchmaker.h/.cpp
│   ├── room.h/.cpp        ← 5자리 코드 커스텀 룸
│   └── relay.h/.cpp       ← 바이트 포워더
│
├── meta/                  ← tetris_meta (HTTP+SQLite 메타/랭킹 서버)
│   ├── main.cpp           ← 진입점, CLI 인자
│   ├── database.h/.cpp    ← SQLite 래퍼
│   ├── api_server.h/.cpp  ← cpp-httplib 엔드포인트
│   ├── http_client.h/.cpp ← 게임/릴레이가 쓰는 HTTP 클라이언트
│   ├── elo.h              ← RP 계산(표준 Elo 기대승률 수식)
│   ├── levels.h           ← XP → 레벨 곡선
│   └── protocol.h         ← JSON 직렬화
│
├── bot/                   ← ONNX Runtime 인-프로세스 봇
│   ├── bot_onnx.h/.cpp    ← Ort::Session 래퍼 (옵션)
│   └── placement.h/.cpp   ← 행동 → 틱 마스크 시퀀스
│
├── bindings/              ← pybind11
│   └── tetris_py.cpp      ← SimGame 노출
│
├── tests/
│   └── sim_hash_dump.cpp  ← 결정론 회귀 테스트 진입점
│
├── python/                ← Python 레이어
│   ├── requirements.txt
│   ├── requirements-colab.txt
│   ├── sim/               ← 네이티브 모듈 래퍼
│   ├── common/            ← 학습·추론 공용
│   ├── netbot/            ← framing/input 패리티 + ONNX export CLI
│   ├── train/             ← Colab 학습 (현재: train_model_zoo_colab.ipynb 단일 진입점)
│   ├── tests/             ← pytest 스위트
│   └── legacy/            ← 이전 Pygame 구현 (비빌드, 참조용)
│
├── third_party/
│   ├── dr_mp3.h           ← 단일 헤더 MP3 디코더 (public domain)
│   ├── stb_truetype.h     ← 단일 헤더 TTF 래스터라이저 (public domain)
│   ├── stb_image.h        ← 단일 헤더 PNG/JPG 디코더 (public domain)
│   ├── httplib.h          ← cpp-httplib (게임/릴레이/메타 HTTP)
│   ├── sqlite3.{c,h}      ← SQLite amalgamation (tetris_meta)
│   └── fetch_onnxruntime.sh
│
├── scripts/               ← 플랫폼별 배포 번들
│   ├── release_win.ps1
│   ├── release_macos.sh
│   └── release_linux.sh
│
├── docs/blog/             ← 이 문서 시리즈
├── Font/                  ← NanumGothic.ttf (한글 TTF), monogram.ttf
├── Sounds/                ← music.mp3, rotate.mp3, clear.mp3 (drop/garbage 효과음은 있으면 로드)
└── assets/                ← images.cfg + icons/player.png, opponent.png, bot.png
```

한 줄 책임 정리:

| 디렉토리 | 책임 | 외부 의존 |
|---|---|---|
| `core/` | 순수 C++ 헬퍼(RNG·해시·상수·입력 비트마스크·리플레이) | 없음 |
| `src/` | 테트리스 로직 + 렌더링 래퍼 + 진입점 | `core/`, `renderer/`, `net/` |
| `platform/` | OS 창/입력 추상화 (`platform.h` 한 인터페이스, 구현 2개) | Win32 API 또는 SDL2 |
| `renderer/` | CPU 2D (ARGB32·사각형·텍스트·이미지·셰이크) | `stb_truetype`, `platform/` |
| `audio/` | MP3 로드 + 재생 (공용 헤더, 백엔드 2개) | XAudio2 또는 SDL2_audio, `third_party/dr_mp3.h` |
| `net/` | TCP 소켓 → 메시지 프레이밍 → lockstep 세션 | WinSock2 또는 BSD 소켓 + pthread |
| `server/` | `tetris_relay` 바이너리: 매치메이킹 + 바이트 릴레이 | `net/` + `meta/http_client.cpp` + `third_party/httplib.h` |
| `meta/` | `tetris_meta` 바이너리: HTTP+SQLite 메타/랭킹 + 게임·릴레이용 HTTP 클라이언트 | `third_party/sqlite3.c`, `third_party/httplib.h` |
| `bot/` | `ORT::Session` 로 학습된 정책 추론 | ONNX Runtime (옵션) |
| `bindings/` | `SimGame` 을 pybind11 모듈 `tetris_py` 로 노출 | pybind11 |
| `python/` | 학습·export, framing/placement 패리티, pytest | 기본: numpy, pytest, pybind11 / 학습·export: torch, gymnasium, onnx, onnxscript |
| `third_party/` | 벤더링된 단일 헤더 + 외부 바이너리 설치 스크립트 | — |
| `scripts/` | 플랫폼별 배포 번들 빌더 (`.zip`/`.tar.gz`/`.app`) | — |
| `docs/` | 블로그 및 설계 문서 | — |

이 구조에서 **화살표는 항상 아래로만 흐른다**. `core/` 는 어디도 import 하지 않고, `src/` 는 `core/` 와 `renderer/` 와 `net/` 을 쓰지만 그 반대는 없다. `server/` 는 `net/` 만 건드리지 `src/` 는 절대 건드리지 않는다 — 이 덕분에 릴레이 서버는 GUI/오디오 없이 빌드할 수 있고 Termux 에서도 돈다. `python/` 은 `bindings/` 를 거쳐 `SimGame` 에만 닿는다 — 렌더링과 네트워크는 Python 관점에서 보이지 않는다.

---

## 2. 의존성 총정리

각 의존성이 **언제 필요하고**, **어떻게 확보**하며, **어느 타겟에 링크**되는지 한 표로 정리한다.

### 2.1 플랫폼 내장 (설치 불필요)

| 라이브러리 | 플랫폼 | 링크 이름 | 쓰임 |
|---|---|---|---|
| GDI / GDI+ | Windows | `gdi32`, `gdiplus` | CPU framebuffer 표시, PNG 로딩 |
| WinMM | Windows | `winmm` | `timeBeginPeriod` 로 고해상도 타이머 |
| WinSock2 | Windows | `ws2_32` | TCP 소켓 |
| XAudio2 | Windows | `xaudio2`, `ole32` | 오디오 재생 |
| pthread | Linux/macOS | `Threads::Threads` | `std::thread` 런타임 |

Windows 에서는 Visual Studio 를 설치하면 위 항목이 SDK 에 들어 있다. Linux 의 pthread 는 glibc 에 들어 있고, macOS 는 Xcode command-line tools 로 C++ toolchain 을 제공한다. 클라이언트는 OpenGL/DirectX/Vulkan 개발 패키지를 요구하지 않는다.

### 2.2 외부 라이브러리

**SDL2** — macOS/Linux 의 창·입력, 그리고 Linux 의 오디오.

- Windows: 기본 비활성 (Win32 경로 사용). `-DTETRIS_USE_SDL2=ON` 으로 활성화 시 `-DSDL2_DIR=...` 로 위치 지정.
- macOS: `brew install sdl2`
- Linux: `apt install libsdl2-dev`
- CMake 에서는 `find_package(SDL2 REQUIRED)` 로 탐색. 배포 버전에 따라 `SDL2::SDL2` 타겟이 있을 수도 있고 `${SDL2_LIBRARIES}` 변수만 제공할 수도 있어, CMakeLists 는 두 경로를 모두 지원한다.

**pybind11** — `tetris_py` 네이티브 모듈 빌드에만 필요.

- `pip install pybind11` 로 설치. 저장소에는 서브모듈로 벤더링돼 있지 않다.
- CMake 에서 `find_package(pybind11 CONFIG QUIET)` 로 탐색. 없으면 `FATAL_ERROR` 로 "`-Dpybind11_DIR=$(python -m pybind11 --cmakedir)` 를 넘겨라" 는 힌트를 준다.
- CMake 4.0+ 는 `FindPythonInterp` / `FindPythonLibs` 가 삭제됐으므로, `set(PYBIND11_FINDPYTHON ON)` 으로 모던 `FindPython` 을 사용하도록 힌트.

**ONNX Runtime** — 봇(`Single vs Bot`) 의 CPU 추론 전용. 용량 때문에 git 서브모듈 대신 **별도 스크립트로 다운로드**한다 (후술 §5).

- 공식 GitHub release 에서 CPU 빌드만 벤더링: Windows `.zip`, macOS `.tgz`(universal2), Linux `.tgz`(x64 또는 aarch64).
- `third_party/onnxruntime/include/onnxruntime_cxx_api.h` 가 있어야 `TETRIS_BUILD_BOT=ON` 이 성공.

**dr_mp3** — 단일 헤더 MP3 디코더 (public domain). `third_party/dr_mp3.h` 로 **이미 저장소에 벤더링**돼 있다. `audio/audio.cpp` 와 `audio/sdl_audio.cpp` 양쪽에서 `#include "../third_party/dr_mp3.h"` 로 사용한다.

**stb 계열** — `third_party/stb_truetype.h` 와 `third_party/stb_image.h` 가 **저장소에 벤더링**돼 있다 (둘 다 public domain 단일 헤더). `renderer/text_software.cpp` 가 모든 플랫폼에서 `stb_truetype` 로 TTF 를 CPU coverage bitmap으로 래스터화하고 직접 알파 합성한다. `renderer/image.cpp` 의 비-Win32 분기는 `stb_image` 로 PNG/JPG 를 디코딩한다 (Windows 는 GDI+ 사용). 각 헤더는 정확히 한 TU 에서 `STB_TRUETYPE_IMPLEMENTATION` / `STB_IMAGE_IMPLEMENTATION` 매크로와 함께 include 된다.

### 2.3 Python 환경

저장소 루트에는 `pyproject.toml` 이 있다. 로컬 기본 환경은 가볍게 유지하고,
PyTorch/Gymnasium/ONNX 는 학습·export extra 로만 설치한다. Mac mini 같은 배포
머신에서 torch 를 끌어오지 않기 위해서다.

```text
pyproject.toml                 → numpy 기본, pytest/pybind11 dev, torch/gymnasium/onnx/onnxscript extra
python/requirements.txt        → pip fallback: numpy, pytest
python/requirements-colab.txt  → requirements.txt + pybind11 + torch + gymnasium + onnx + onnxscript
```

루트 `pyproject.toml` 의 핵심은 다음이다.

```toml
dependencies = [
    "numpy>=1.24",
]

[project.optional-dependencies]
train = [
    "gymnasium>=0.29",
    "torch>=2.1",
]
export = [
    "onnx>=1.14",
    "onnxscript>=0.1",
    "torch>=2.1",
]
```

[uv](https://github.com/astral-sh/uv) 는 이 형식을 읽고 자동으로 `.venv/` 를 만들어 준다.
프로젝트 컨벤션은 저장소 루트에서 실행하는 것이다.

```bash
uv sync --dev
uv run python -m pytest python/tests
```

`uv sync --dev` 는 torch 를 설치하지 않는다. `.pt` checkpoint 를 직접 실행하거나
`.pt -> .onnx` export 를 해야 할 때만 Colab 또는 별도 학습 머신에서 extra 를 켠다.

```bash
uv sync --dev --extra train --extra export
```

### 2.4 타겟별 의존성 매트릭스

| 타겟 | CPU 렌더러 | SDL2 | Win32 API | ONNX RT | pybind11 | 필요 조건 |
|---|---|---|---|---|---|---|
| `tetris` (Win32 경로) | ✓ | — | ✓ | 옵션 | — | Windows only |
| `tetris` (SDL2 경로) | ✓ | ✓ | — | 옵션 | — | 전 플랫폼 |
| `tetris_relay` | — | — | ws2_32만 | — | — | 헤드리스, Termux OK |
| `sim_hash_dump` | — | — | — | — | — | 결정론 테스트, 전 플랫폼 |
| `tetris_py` (pybind11) | — | — | — | — | ✓ | Colab/로컬 Python |

이 표가 CMakeLists 의 옵션 플래그 설계를 결정한다.

---

## 3. CMakeLists.txt 해부

이 장에서는 `CMakeLists.txt` 를 섹션별로 발췌하며 전부 설명한다.

### 3.1 프롤로그

```cmake
cmake_minimum_required(VERSION 3.15)
# C 언어도 활성화 — third_party/sqlite3.c (amalgamation) 를 빌드하려면 필요.
project(tetris CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: UTF-8 소스 파일 인코딩 (한국어 주석 깨짐 방지)
if (MSVC)
    add_compile_options(/utf-8)
endif()
```

CMake 3.15 는 `find_package` 에 `CONFIG` 모드, `target_link_libraries` 에 타겟 기반 의존성 같은 현대적 기능을 안정적으로 지원하는 최저선이다. C++17 은 `std::optional`, structured binding, `if constexpr` 를 쓰기 위해 필수.

MSVC 의 `/utf-8` 는 소스/실행 인코딩 모두 UTF-8 로 설정하는 플래그다. 이 저장소는 C++ 주석이 한국어로 많이 적혀 있고, MSVC 가 기본으로 가정하는 시스템 로케일(CP949 등)에서 컴파일하면 `warning C4819` 가 쏟아진다. `/utf-8` 하나로 전부 해결.

### 3.2 옵션 플래그

빌드 옵션은 타겟 단위 빌드 토글 5개, 기능 토글 3개, 그리고 백엔드 1개 + 봇 1개로 총 10개다 (이 외에 경로/문자열 캐시 변수도 몇 개 있다):

```cmake
option(TETRIS_BUILD_GAME  "Build the handmade game executable"              ON)
option(TETRIS_BUILD_PY    "Build the pybind11 module (tetris_py)"           OFF)
option(TETRIS_BUILD_TEST  "Build the SimGame determinism test"              ON)
option(TETRIS_BUILD_RELAY "Build the tetris_relay matchmaking server"       OFF)
option(TETRIS_BUILD_META  "Build the tetris_meta HTTP+SQLite metadata server" OFF)
option(TETRIS_BUILD_BOT   "Link onnxruntime (Section C bot inference)"      OFF)

option(TETRIS_ENABLE_HTTPS     "Enable HTTPS for tetris_meta clients when OpenSSL is available" ON)
option(TETRIS_ENABLE_DEBUG_UI  "Enable in-game debug overlays in the game client"               OFF)
option(TETRIS_ENABLE_NET_TRACE "Enable verbose game-client net/session trace logs"             OFF)

if (WIN32)
    option(TETRIS_USE_SDL2 "Use SDL2 backend (cross-platform)" OFF)
else()
    option(TETRIS_USE_SDL2 "Use SDL2 backend (cross-platform)" ON)
endif()
```

기능 토글 셋은 다음과 같다:

- **`TETRIS_ENABLE_HTTPS`** — 기본 **ON**. 켜져 있고 OpenSSL 이 발견되면 메타 클라이언트가 `https://` URL 을 쓸 수 있다 (`CPPHTTPLIB_OPENSSL_SUPPORT` + `OpenSSL::SSL`/`OpenSSL::Crypto` 링크). 즉 이 옵션은 **OpenSSL 을 끌어올 수 있다**. OpenSSL 이 없으면 경고만 내고 런타임에 `https` URL 을 거부한다.
- **`TETRIS_ENABLE_DEBUG_UI`** — 기본 OFF. 게임 클라이언트의 인게임 디버그 오버레이를 켠다 (`TETRIS_ENABLE_DEBUG_UI=1` 매크로).
- **`TETRIS_ENABLE_NET_TRACE`** — 기본 OFF. net/session 의 상세 추적 로그를 켠다.

각 플래그의 의미:

- **`TETRIS_BUILD_GAME`** — 게임 실행 파일(`tetris`). 기본 ON. Windows 에서는 Win32 경로, 그 외는 SDL2 경로로 빌드된다.
- **`TETRIS_BUILD_PY`** — pybind11 모듈(`tetris_py`). 기본 OFF — Colab 학습
  환경이나 native Sim 테스트에 사용한다. 배포 클라이언트와 인게임 ONNX 봇에는
  필요 없다.
- **`TETRIS_BUILD_TEST`** — `sim_hash_dump` 결정론 회귀 테스트. 기본 ON — GUI 가 없으므로 어느 플랫폼에서든 빌드된다.
- **`TETRIS_BUILD_RELAY`** — `tetris_relay` 매치메이킹 서버. 기본 OFF — 릴레이 호스트에서만 켠다.
- **`TETRIS_BUILD_META`** — `tetris_meta` HTTP+SQLite 메타/RP 서버. 기본 OFF — 별도 호스트에서만 켠다.
- **`TETRIS_BUILD_BOT`** — ONNX Runtime 링크. OFF 라도 `bot/bot_onnx.cpp` 는 컴파일되지만 `TETRIS_HAS_ONNXRUNTIME` 매크로가 미정의라 **스텁 모드**로 빌드돼, ONNX 모델에 대한 `Load()` 가 실패한다. 다만 "Single vs Bot" 자체는 내장 휴리스틱 봇으로 항상 열 수 있고, 학습 모델만 ONNX Runtime 이 있을 때 추가로 선택 가능하다.
- **`TETRIS_USE_SDL2`** — 백엔드 선택. Windows 는 OFF(= Win32 handmade), 그 외는 ON(= SDL2). 이 값이 `platform/*.cpp`, `renderer/text_*.cpp`, `audio/*.cpp` 세 쌍의 선택을 동시에 결정한다.

CMake 명령줄에서는 `-DTETRIS_BUILD_RELAY=ON` 처럼 넘긴다.

### 3.3 공유 소스 목록

모든 타겟이 쓰는 순수 시뮬 파일들을 변수로 뽑아 둔다:

```cmake
# Pure (no raylib) logic — used by game, pybind11 module, and tests.
set(TETRIS_SIM_SOURCES
    src/sim_game.cpp
    src/position.cpp
)

set(TETRIS_SIM_HEADERS
    src/sim_game.h
    src/sim_grid.h
    src/sim_block.h
    src/sim_blocks.h
    src/position.h
    core/constants.h
    core/input.h
    core/rng.h
    core/hash.h
)
```

`sim_grid.h` / `sim_block.h` / `sim_blocks.h` 가 헤더만 있는 이유는 이들이 템플릿 없이 구조체 + inline 함수만 담기 때문이다 (Part 1 에서 다룬다). `SimGame` 은 `.cpp` 로 분리했는데, 이 파일은 크고 RNG/해시 구현이 담겨 단일 번역 단위로 두는 게 빌드 시간상 이득이다.

### 3.4 타겟 1 — `tetris` (게임 클라이언트)

이 섹션은 `TETRIS_BUILD_GAME=ON` 일 때만 활성화된다. 내부 구조는 3단계다:

(a) **공통 소스 묶음** — 백엔드와 무관하게 항상 포함:

```cmake
if (TETRIS_BUILD_GAME)
    set(TETRIS_GAME_COMMON
        ${TETRIS_SIM_SOURCES}
        src/main.cpp
        src/game.cpp
        src/gui.cpp
        src/colors.cpp
        core/replay.cpp
        net/socket.cpp
        net/framing.cpp
        net/session.cpp
        renderer/renderer.cpp
        renderer/text_software.cpp
        renderer/shake.cpp
        renderer/image.cpp
        bot/placement.cpp
        bot/bot_onnx.cpp
        meta/http_client.cpp
    )
```

주목할 점: `bot/bot_onnx.cpp` 는 `TETRIS_BUILD_BOT=OFF` 라도 **항상 컴파일된다**. ONNX 모델을 고르면 런타임에 `Load()` 가 실패하지만, 내장 휴리스틱 봇은 이 파일과 무관하게 동작한다. 이 덕분에 `main.cpp` 의 `#ifdef` 분기가 필요 없다 — 호출 쪽 코드는 항상 동일하고, 빌드 옵션은 "학습 모델을 ONNX Runtime 으로 로드할 수 있는가" 만 바꾼다. `meta/http_client.cpp` 는 게임 클라이언트의 guest 토큰 발급/검증 경로에서 쓰인다.

**중요:** `meta/http_client.cpp` 가 게임 공통 소스에 들어가므로, 기본 게임 빌드(`TETRIS_BUILD_GAME=ON`)에도 `third_party/httplib.h` 가 **반드시** 있어야 한다. `CMakeLists.txt` 는 이 블록 앞에서 헤더 존재를 검사하고, 없으면 게임 타깃에 대해서도 `FATAL_ERROR` 로 즉시 멈춘다:

```cmake
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_GAME=ON 이지만 third_party/httplib.h 가 없습니다. "
            "tetris_meta 서버 호출 (guest 토큰) 용. 다운로드 후 재시도.")
    endif()
```

즉 `httplib.h` 는 릴레이/메타 전용이 아니라 **기본 게임 빌드의 선행 조건**이기도 하다.

(b) **백엔드 분기** — 소프트웨어 렌더러와 텍스트는 공통이고,
`TETRIS_USE_SDL2` 에 따라 창/표시와 오디오 2개 파일만 교체:

```cmake
    if (TETRIS_USE_SDL2)
        find_package(SDL2 REQUIRED)

        add_executable(tetris
            ${TETRIS_GAME_COMMON}
            ${TETRIS_GAME_HEADERS}
            platform/sdl.cpp
            audio/sdl_audio.cpp
        )
        target_include_directories(tetris PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            ${SDL2_INCLUDE_DIRS})

        # SDL2::SDL2 타겟은 find_package(SDL2) 배포 버전마다 제공 여부가 다름
        if (TARGET SDL2::SDL2)
            target_link_libraries(tetris PRIVATE SDL2::SDL2)
        else()
            target_link_libraries(tetris PRIVATE ${SDL2_LIBRARIES})
        endif()

        if (WIN32)
            target_link_libraries(tetris PRIVATE gdiplus ws2_32)
        elseif (NOT APPLE)
            find_package(Threads REQUIRED)
            target_link_libraries(tetris PRIVATE Threads::Threads)
        endif()
    else()
        # Handmade 경로: Win32 window/presentation + XAudio2
        add_executable(tetris
            ${TETRIS_GAME_COMMON}
            ${TETRIS_GAME_HEADERS}
            platform/win32.cpp
            audio/audio.cpp
        )
        target_include_directories(tetris PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

        if (WIN32)
            target_link_libraries(tetris PRIVATE gdi32 gdiplus winmm ws2_32 xaudio2 ole32)
        else()
            message(FATAL_ERROR "Handmade Win32 backend is Windows-only. Set -DTETRIS_USE_SDL2=ON.")
        endif()
    endif()
```

두 분기의 **짝** 관계를 그림으로 정리하면:

| 백엔드 | `platform/` | `renderer/` | `audio/` |
|--------|-------------|------------|----------|
| Win32  | `win32.cpp` | `renderer.cpp` + `text_software.cpp` | `audio.cpp` |
| SDL2   | `sdl.cpp`   | 같은 공통 소프트웨어 렌더러 | `sdl_audio.cpp` |

`TETRIS_USE_SDL2` 값은 플랫폼 표시와 오디오만 바꾼다. CPU rasterization,
글리프 캐시, 이미지 sampling 코드는 두 경로에서 완전히 동일하다.

헤더 `platform/platform.h`, `renderer/renderer.h`, `audio/audio.h` 는 양쪽이 동일한 인터페이스를 구현한다. 그래서 `src/main.cpp`, `src/game.cpp` 는 **한 줄도 바뀌지 않는다** — 선택은 전적으로 CMake 레벨.

Win32 경로의 링크 목록을 한 줄씩 훑어보자:

- `gdi32` — memory DIB backbuffer, `StretchDIBits`, `BitBlt`.
- `gdiplus` — `Gdiplus::Bitmap` PNG 로더 (`renderer/image.cpp`).
- `winmm` — `timeBeginPeriod(1)` 로 `Sleep` 해상도 1ms 강제.
- `ws2_32` — WinSock2 (`socket`, `connect`, `send`, `recv`).
- `xaudio2` — `IXAudio2CreateCom` 상위 인터페이스.
- `ole32` — `CoInitializeEx` (XAudio2 가 COM 위에 있음).

SDL2 경로 Linux 분기에서 `find_package(Threads REQUIRED)` 이 필요한 이유: `std::thread` 는 C++ 표준이지만 GCC/libstdc++ 는 내부적으로 pthread 를 호출한다. 대부분의 배포판에서는 `-lpthread` 를 걸지 않으면 `undefined reference to pthread_create` 로 링크 실패한다. `Threads::Threads` 타겟이 이 플래그를 자동으로 붙여준다.

(c) **선택적 ONNX Runtime** — `TETRIS_BUILD_BOT=ON` 이 켜졌을 때만:

```cmake
    if (TETRIS_BUILD_BOT)
        set(ORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
        if (NOT EXISTS "${ORT_ROOT}/include/onnxruntime_cxx_api.h")
            message(FATAL_ERROR
                "TETRIS_BUILD_BOT=ON 이지만 ${ORT_ROOT}/include/onnxruntime_cxx_api.h 가 없습니다. "
                "third_party/fetch_onnxruntime.sh 로 벤더링하거나 TETRIS_BUILD_BOT=OFF 로 빌드하세요.")
        endif()
        target_compile_definitions(tetris PRIVATE TETRIS_HAS_ONNXRUNTIME=1)
        target_include_directories(tetris PRIVATE "${ORT_ROOT}/include")
        if (WIN32)
            target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/win-x64/onnxruntime.lib")
        elseif (APPLE)
            target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/osx-universal2/libonnxruntime.dylib")
        else()
            target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/linux-x64/libonnxruntime.so")
        endif()
    endif()
```

`TETRIS_HAS_ONNXRUNTIME=1` 매크로가 정의되면 `bot/bot_onnx.cpp` 가 실제 `Ort::Session` 경로로 컴파일된다 (정의 안 되면 스텁). CMake 는 **헤더 존재 여부만 사전 검사**하고, 그마저도 없으면 친절히 `fetch_onnxruntime.sh` 를 가리키는 에러로 실패한다.

(d) **rpath & .app 번들 메타** — 배포용 설정:

```cmake
    if (APPLE)
        set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.rein.tetris")
        set(MACOSX_BUNDLE_BUNDLE_NAME "Tetris")
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in")
            if (NOT DEFINED PROJECT_VERSION)
                set(PROJECT_VERSION "1.0.0")
            endif()
            configure_file(
                "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in"
                "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
                @ONLY)
        endif()
        set_target_properties(tetris PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks")
    elseif (UNIX)
        set_target_properties(tetris PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()
```

**rpath 가 왜 중요한가.** macOS 와 Linux 의 동적 링커(`dyld`, `ld-linux`)는 실행 파일이 필요로 하는 `.dylib`/`.so` 를 시스템 경로(`/usr/lib`, `/usr/local/lib`)에서 찾는다. 하지만 배포 번들은 시스템에 아무것도 설치하지 않고 동봉된 라이브러리를 쓰고 싶다. rpath 는 실행 파일 안에 임베드되는 "탐색 경로 힌트" 다:

- macOS: `@executable_path/../Frameworks` — `Tetris.app/Contents/MacOS/tetris` 에서 `Tetris.app/Contents/Frameworks/libSDL2.dylib` 를 찾아간다.
- Linux: `$ORIGIN/lib` — 실행 파일과 같은 폴더의 `lib/libSDL2.so` 를 찾아간다.

Windows 는 rpath 개념이 없다 — DLL 은 "실행 파일과 같은 폴더" 를 자동으로 뒤지므로 배포 번들에서 DLL 을 `tetris.exe` 옆에 두기만 하면 된다.

### 3.5 `copy_assets` 커스텀 타겟

실행 파일은 빌드 디렉토리에 생성되지만 `Font/NanumGothic.ttf` 나 `Sounds/music.mp3` 는 소스 디렉토리에 있다. 게임은 상대 경로 `Font/...` 로 리소스를 여는데, 빌드 디렉토리에서 실행하면 파일을 못 찾는다. 해결은 빌드 시 자동으로 복사하는 커스텀 타겟이다:

```cmake
    set(_copy_cmds
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Font   ${CMAKE_CURRENT_BINARY_DIR}/Font
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Sounds ${CMAKE_CURRENT_BINARY_DIR}/Sounds
    )
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        list(APPEND _copy_cmds
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
    endif()
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model")
        list(APPEND _copy_cmds
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/model ${CMAKE_CURRENT_BINARY_DIR}/model)
    endif()
    add_custom_target(copy_assets ALL
        ${_copy_cmds}
        DEPENDS tetris
    )
endif()
```

핵심은 세 가지:

1. **`${CMAKE_COMMAND} -E copy_directory`** — CMake 자체의 플랫폼 독립 `cp -R`. `cp` / `robocopy` 로 분기할 필요 없음.
2. **`ALL`** — 기본 빌드에 포함(타겟 이름을 명시하지 않아도 실행). 디폴트 `cmake --build` 한 방에 따라온다.
3. **`DEPENDS tetris`** — 실행 파일이 먼저 빌드된 후 복사. 병렬 빌드 시에도 순서 보장.

`assets/`, `model/` 은 **없을 수도 있다**. `assets/` 에는 `images.cfg` 와 아이콘/콜아웃 PNG 가, `model/` 에는 배포용 ONNX 모델과 선택적 `bots.cfg` 가 들어간다. 권장 경로는 `model/bots/*.onnx` 이고, 예전 단일 모델 배포를 위해 legacy `model/*.onnx` 도 스캔한다. 둘 다 선택적이므로 `if (EXISTS ...)` 로 조건부 추가해 에러를 막는다.

### 3.6 타겟 2 — `tetris_relay` (릴레이 서버)

```cmake
if (TETRIS_BUILD_RELAY)
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_RELAY=ON 이지만 third_party/httplib.h 가 없습니다. "
            "meta API 호출용. cpp-httplib 를 다운로드해 third_party/ 에 넣으세요.")
    endif()
    add_executable(tetris_relay
        server/main.cpp
        server/matchmaker.cpp
        server/player_conn.cpp
        server/relay.cpp
        server/room.cpp
        net/socket.cpp
        net/framing.cpp
        meta/http_client.cpp
        server/matchmaker.h
        server/player_conn.h
        server/relay.h
        server/room.h
        net/socket.h
        net/framing.h
        meta/http_client.h
        meta/protocol.h
    )
    target_include_directories(tetris_relay PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    if (WIN32)
        target_link_libraries(tetris_relay PRIVATE ws2_32)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(tetris_relay PRIVATE Threads::Threads)
    endif()
    if (UNIX AND NOT APPLE)
        set_target_properties(tetris_relay PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()
endif()
```

주목: 소스 목록에 `src/` 가 **한 파일도 없다**. `server/` + `net/` + `meta/http_client.cpp` 뿐이다. OpenGL 도, SDL2 도 없다. 다만 ranked 매치에서 meta API 를 호출할 수 있어야 하므로 `third_party/httplib.h` 는 릴레이 단독 빌드에도 필요하다.

### 3.7 타겟 3 — `tetris_meta` (HTTP+SQLite 메타 서버)

```cmake
if (TETRIS_BUILD_META)
    add_executable(tetris_meta
        meta/main.cpp
        meta/database.cpp
        meta/api_server.cpp
        third_party/sqlite3.c
        meta/database.h
        meta/api_server.h
        meta/elo.h
        meta/protocol.h
    )
    target_include_directories(tetris_meta PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_compile_definitions(tetris_meta PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_RTREE=0
        SQLITE_DEFAULT_FOREIGN_KEYS=1)
endif()
```

`tetris_meta` 는 게임 클라이언트와 독립된 HTTP+SQLite 프로세스다. 실행 인자는 현재 코드 기준 `--db PATH`, `--http HOST:PORT`, `--relay-secret SECRET`, `--allow-public-matches` 이며, 기본값은 `tetris.db` 와 `127.0.0.1:8080` 이다. 운영에서는 Caddy/Tunnel 뒤에 두고 `/v1/matches` 에 `X-Relay-Secret` 을 요구한다. secret 이 없으면 기본적으로 시작하지 않고, `--allow-public-matches` 는 로컬 테스트 전용이다.

### 3.8 타겟 4 — `sim_hash_dump` (결정론 테스트)

```cmake
if (TETRIS_BUILD_TEST)
    add_executable(sim_hash_dump
        tests/sim_hash_dump.cpp
        ${TETRIS_SIM_SOURCES}
        ${TETRIS_SIM_HEADERS}
    )
    target_include_directories(sim_hash_dump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
```

오직 순수 시뮬만 링크. OS API 없음, 네트워크 없음. 이 바이너리는 고정 시드 + 고정 입력 시퀀스를 1000틱 돌려 매 틱마다 `StateHash()` 를 stdout 에 뱉는다. Python 쪽에도 같은 시드로 생성한 레퍼런스 덤프(`python/tests/_sim_hash_dump.txt`)가 있어, `diff <(./sim_hash_dump) python/tests/_sim_hash_dump.txt` 한 줄로 결정론 회귀를 검증한다. 플랫폼 간 `StateHash` 가 한 비트라도 다르면 멀티플레이가 desync 된다 — 이 바이너리가 마지막 방어선.

### 3.9 타겟 5 — `tetris_py` (pybind11 모듈)

```cmake
if (TETRIS_BUILD_PY)
    set(PYBIND11_FINDPYTHON ON)
    find_package(pybind11 CONFIG QUIET)
    if (NOT pybind11_FOUND)
        message(FATAL_ERROR
            "pybind11 not found. Install it (pip install pybind11) and "
            "re-run cmake with -Dpybind11_DIR=$(python -m pybind11 --cmakedir)")
    endif()

    pybind11_add_module(tetris_py
        bindings/tetris_py.cpp
        ${TETRIS_SIM_SOURCES}
        ${TETRIS_SIM_HEADERS}
    )

    target_include_directories(tetris_py PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
```

`pybind11_add_module` 매크로는 pybind11 이 제공하는 헬퍼로, 다음을 알아서 해준다:

- 올바른 shared library 접미사(Linux `.so`, macOS `.so`, Windows `.pyd`)
- Python 헤더 include
- Python ABI 에 맞는 심볼 내보내기 설정(`-fvisibility=hidden` + `PYBIND11_MODULE`)
- Python 인터프리터 자동 감지(CMake 4.0+ 호환을 위해 `PYBIND11_FINDPYTHON` 힌트)

소스에는 `bindings/tetris_py.cpp` + 공유 시뮬 2개가 들어간다. 렌더러·네트워크·플랫폼은 한 줄도 없다. Python 은 `SimGame` 만 보고, 자기 쪽 렌더링/네트워크는 Python 레이어에서 따로 구현한다.

### 3.10 라이브러리 링크 순서는 왜 중요한가

CMake 는 `target_link_libraries` 에 적은 순서대로 링커에 전달한다 (`-lA -lB -lC` 순으로). GCC/Clang 의 정적 링커는 **"왼쪽에서 오른쪽으로" 한 번만** 심볼 테이블을 훑는다. 만약 `A.o` 가 `libB` 의 심볼을 필요로 하면 반드시 `A` 가 `B` 보다 **먼저** 등장해야 한다. 그렇지 않으면 `B` 의 심볼이 그 시점에 필요하지 않은 것으로 판단돼 링커가 건너뛴다.

이 저장소에서는 이 문제가 겉으로 드러나지 않는데, 이유는:

1. `target_link_libraries` 가 받는 항목이 대부분 "누가 참조하는지 명확한 leaf" 들이다. 예: `gdi32` 는 Win32 표시 코드만 직접 사용한다.
2. MSVC 의 링커는 다수의 패스를 돌려 이 순서 민감도가 약하다.
3. `SDL2::SDL2` 같은 IMPORTED 타겟은 내부에 `INTERFACE_LINK_LIBRARIES` 를 달고 있어, CMake 가 자동으로 전이적 의존성을 해결한다.

그래도 관례를 알아두면 좋다: **"사용하는 쪽 → 사용되는 쪽"** 순서다. 예컨대 `target_link_libraries(tetris PRIVATE gdi32 gdiplus winmm ws2_32 xaudio2 ole32)` 에서 맨 뒤의 `ole32` 는 `xaudio2` 가 쓴다(COM). Linux SDL2 경로에서는 `Threads::Threads` 가 `std::thread` 런타임의 pthread 심볼을 제공한다.

---

## 4. `third_party/fetch_onnxruntime.sh` 의 역할

ONNX Runtime CPU 바이너리는 플랫폼별 번들이 크고, 사용자마다 필요한 플랫폼도 다르다(Linux 릴레이 호스트에서 봇을 돌릴 일은 없음). 그래서 Git 서브모듈로 묶지 않고 CMake 밖으로 빼서 **쉘 스크립트 하나**로 벤더링한다.

전체 소스는 다음과 같다:

```bash
#!/usr/bin/env bash
# third_party/fetch_onnxruntime.sh — 공식 ONNX Runtime CPU 릴리스 다운로드.
#
# 사용법:
#   ./third_party/fetch_onnxruntime.sh          # 현재 OS/아키텍처 자동 감지
#   ./third_party/fetch_onnxruntime.sh 1.18.1   # 특정 버전 지정
#
# 완료 후 third_party/onnxruntime/ 에 include/ 과 lib/<platform>/ 이 배치된다.
# CMake -DTETRIS_BUILD_BOT=ON 이 이 구조를 기대한다.
set -euo pipefail

ORT_VERSION="${1:-1.18.1}"
BASE_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}"
DEST="$(cd "$(dirname "$0")" && pwd)/onnxruntime"

detect_platform() {
    local os arch
    os="$(uname -s)"
    arch="$(uname -m)"

    case "$os" in
        Linux)
            case "$arch" in
                x86_64)  echo "linux-x64" ;;
                aarch64) echo "linux-aarch64" ;;
                *)       echo "linux-x64" ;;  # fallback
            esac ;;
        Darwin)
            echo "osx-universal2" ;;
        MINGW*|MSYS*|CYGWIN*|Windows_NT)
            echo "win-x64" ;;
        *)
            echo >&2 "[fetch_onnxruntime] Unknown OS: $os"; exit 1 ;;
    esac
}
```

설계 결정 세 가지를 짚어보자:

**왜 CMake `FetchContent` 나 `ExternalProject_Add` 가 아닌가.** ONNX Runtime 은 공식 배포가 **이미 바이너리** 다 — CMake 빌드 스크립트가 들어 있지 않다. `FetchContent` 로 끌어와도 빌드할 수 없고, 단지 압축을 풀어 경로를 맞추는 일이 전부다. 그 일은 쉘이 더 잘 한다. 또한 CMake 시점에 네트워크 요청을 하면 오프라인 빌드가 깨진다. 스크립트는 한 번 실행하고 결과물을 커밋하지 않은 채 로컬에 남겨두는 편이 관리가 쉽다.

**버전 핀 전략.** 기본값 `1.18.1` 이 스크립트에 하드코딩돼 있다. 첫 번째 인자로 다른 버전(`./fetch_onnxruntime.sh 1.19.0`)을 넘길 수 있지만, CMake 는 이 값을 모른다 — 그저 `include/onnxruntime_cxx_api.h` 와 `lib/<platform>/` 하위의 확장자만 본다. API 레벨의 호환성은 Microsoft 가 세마 버저닝으로 보장한다. 이 프로젝트가 쓰는 `Ort::Session` API 는 대략 1.16 ~ 1.19 범위를 겨냥해 작성했지만, 이는 모든 패치 버전에서 측정한 보장이 아니라 가이드 정도로 본다 — 그래서 보통은 버전을 자주 건드릴 일이 없다.

**파일 배치 규칙.** 스크립트는 아카이브를 풀어 `third_party/onnxruntime/include/` (헤더) 와 `third_party/onnxruntime/lib/<platform>/` (라이브러리) 로 정리한다. `<platform>` 은 `win-x64`, `osx-universal2`, `linux-x64`, `linux-aarch64` 중 하나. CMakeLists 는 이 경로를 직접 참조한다:

```cmake
if (WIN32)
    target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/win-x64/onnxruntime.lib")
elseif (APPLE)
    target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/osx-universal2/libonnxruntime.dylib")
else()
    target_link_libraries(tetris PRIVATE "${ORT_ROOT}/lib/linux-x64/libonnxruntime.so")
endif()
```

CMake 쪽 에러 메시지도 스크립트를 정확히 가리킨다:

```text
FATAL_ERROR: TETRIS_BUILD_BOT=ON 이지만 third_party/onnxruntime/include/onnxruntime_cxx_api.h
가 없습니다. third_party/fetch_onnxruntime.sh 로 벤더링하거나 TETRIS_BUILD_BOT=OFF 로 빌드하세요.
```

Windows PowerShell 에서 bash 가 없다면 WSL 이나 Git Bash 를 써야 한다 — 또는 해당 아카이브를 수동으로 받아 같은 경로에 풀어도 된다.

---

## 5. 첫 빌드 — Windows (Visual Studio 2022)

### 5.1 사전 준비

1. [Visual Studio 2022 Community](https://visualstudio.microsoft.com/) 설치. "Desktop development with C++" 워크로드를 선택 — 여기에 MSVC 툴체인, Windows SDK, CMake 통합이 전부 포함된다.
2. Git for Windows (또는 GitHub Desktop).
3. (선택) 봇 빌드를 할 거라면 Git Bash 도 깔려 있어야 `fetch_onnxruntime.sh` 를 실행할 수 있다.

### 5.2 기본 빌드 (게임 + 테스트)

PowerShell 또는 `x64 Native Tools Command Prompt for VS 2022` 를 띄우고:

```powershell
cd D:\path\to\Tetris-Multiplayer-RL

# configure — build/ 디렉토리 생성, Visual Studio 솔루션 자동 감지
cmake -B build

# build — Release 구성으로 병렬 빌드
cmake --build build --config Release
```

첫 `cmake -B build` 는 다음을 찍는다 (일부):

```text
-- Selecting Windows SDK version 10.0.xxxxx to target Windows 10.0.
-- The CXX compiler identification is MSVC 19.xx.xxxxx
-- Detecting CXX compile features
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

실행 파일은 `build\Release\tetris.exe`, `build\Release\sim_hash_dump.exe` 에 생성된다. `build\Release\` 에 `Font\` 와 `Sounds\` 폴더도 자동 복사됐는지 확인한다 (`copy_assets` 타겟의 효과).

```powershell
.\build\Release\tetris.exe
.\build\Release\sim_hash_dump.exe
```

전자는 게임 창, 후자는 해시 덤프 텍스트가 stdout 에 쏟아진다.

### 5.3 릴레이 서버까지 포함

```powershell
cmake -B build -DTETRIS_BUILD_RELAY=ON
cmake --build build --config Release

.\build\Release\tetris_relay.exe --port 7777
```

### 5.4 봇 포함 (ONNX Runtime 벤더링 필요)

Git Bash 에서:

```bash
./third_party/fetch_onnxruntime.sh
```

그 후 PowerShell 로 돌아가:

```powershell
cmake -B build -DTETRIS_BUILD_BOT=ON
cmake --build build --config Release

# 실행 전에 onnxruntime.dll 을 exe 옆에 복사
copy .\third_party\onnxruntime\lib\win-x64\onnxruntime.dll .\build\Release\

.\build\Release\tetris.exe
```

메뉴에서 "Single vs Bot" 을 열고 `Heuristic (test)` 가 보이면 성공이다. 학습 모델 파일이 없어도 이 휴리스틱 봇으로 오프라인 대전을 시작할 수 있다. Colab 에서 export 한 `.onnx` 를 `model/bots/*.onnx` 에 두면 같은 화면에 추가로 나타난다 — Part 9 에서 다룬다.

### 5.5 배포 번들 한 방에

```powershell
.\scripts\release_win.ps1              # 기본
.\scripts\release_win.ps1 -Bot         # 봇 포함
.\scripts\release_win.ps1 -Sdl2        # SDL2 백엔드
```

`dist\tetris-win-x64.zip` 이 생성된다 — 압축 해제 후 `tetris.exe` 더블클릭이면 끝.

---

## 6. 첫 빌드 — Linux / macOS (SDL2 백엔드)

### 6.1 Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y cmake g++ libsdl2-dev

git clone <repo> tetris && cd tetris
cmake -B build
cmake --build build -j$(nproc)

./build/tetris
./build/sim_hash_dump
```

- `cmake -B build` 는 Windows 가 아니므로 `TETRIS_USE_SDL2=ON` 을 기본값으로 잡는다.
- 멀티플레이 테스트를 하려면 릴레이 서버를 먼저 띄운다: `./build/tetris_relay --port 7777` (`-DTETRIS_BUILD_RELAY=ON` 옵션으로 빌드 먼저).
- 헤드리스 서버 전용 빌드는 `cmake -B build -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON` — SDL2 가 필요 없으므로 `libsdl2-dev` 설치도 생략할 수 있다.

저장소 루트에는 이 cmake 호출들을 감싼 얇은 최상위 `Makefile` 도 있다. `make`(=`all`/`build`), `make configure`, `make game`, `make test`, `make relay`, `make meta`, `make py` 타깃으로 같은 빌드를 더 짧게 부를 수 있다 (각 타깃이 필요한 `-D...` 옵션을 자동으로 붙인다).

### 6.2 macOS (Apple Silicon / Intel)

```bash
brew install cmake sdl2

git clone <repo> tetris && cd tetris
cmake -B build
cmake --build build -j$(sysctl -n hw.ncpu)

./build/tetris
```

macOS 에서도 SDL2 경로가 기본이다. `cmake -B build -DTETRIS_USE_SDL2=ON` 을 명시적으로 넘겨도 결과는 동일. OpenGL framework는 링크하지 않으며, CMake는 배포용 rpath를 `@executable_path/../Frameworks` 로 설정한다.

`.app` 번들이 필요하면:

```bash
./scripts/release_macos.sh             # Universal(arm64 + x86_64) .app 생성
open dist/Tetris.app
```

### 6.3 Termux (Android) — 릴레이 서버 전용

Android 휴대폰에서 릴레이를 돌리는 시나리오:

```bash
# Termux 에서 (Android 앱)
pkg install proot-distro
proot-distro install ubuntu
proot-distro login ubuntu

# 이제 Ubuntu proot 안:
apt update && apt install -y cmake g++ git
git clone <repo> tetris && cd tetris

cmake -B build \
    -DTETRIS_BUILD_GAME=OFF \
    -DTETRIS_BUILD_RELAY=ON \
    -DTETRIS_BUILD_TEST=OFF
cmake --build build -j4

./build/tetris_relay --port 7777
```

GUI/오디오/클라이언트 렌더링 의존성이 모두 제거돼 `apt install cmake g++` 두 패키지면 빌드된다. 공유 Wi-Fi 상의 클라이언트는 휴대폰 내부 IP(보통 `192.168.x.x`)로 접속 가능.

---

## 7. Python 환경 — uv

### 7.1 uv 설치

```bash
# macOS / Linux
curl -LsSf https://astral.sh/uv/install.sh | sh

# Windows (PowerShell)
irm https://astral.sh/uv/install.ps1 | iex
```

설치 후 `uv --version` 으로 확인.

### 7.2 가상환경 동기화

저장소 루트에서:

```bash
uv sync --dev
```

이 한 줄이 다음을 자동 수행한다:

1. 루트 `pyproject.toml` 을 읽어 Python 3.12+ 환경을 만든다.
2. 기본 dependency `numpy` 와 dev dependency `pytest`, `pybind11` 을 설치한다.
3. torch, gymnasium, onnx, onnxscript 는 설치하지 않는다.

### 7.3 pytest 실행

```bash
uv run python -m pytest python/tests -v
```

`python/tests/` 에는 다음과 같은 테스트들이 있다 (일부):

- `test_framing_parity.py` — Python framing 구현을 고정 wire 벡터와 대조.
- `test_placement_parity.py` — Python `expand_placement`의 C++ 포트 기준 진리표와 불변식 검증.
- `test_room_smoke.py` — 릴레이 서버에 두 클라이언트를 붙여 5자리 코드 룸이 페어링되는지.
- `test_determinism_crossplatform.py` / `test_checkpoint_roundtrip.py` / `test_meta_db_smoke.py` / `test_relay_meta_smoke.py` 등도 함께 들어 있다.

여기에 C++ 결정론 덤프를 대조하려면:

```bash
./build/sim_hash_dump > /tmp/sim_out.txt
diff /tmp/sim_out.txt python/tests/_sim_hash_dump.txt
```

출력 없이 종료되면 통과.

### 7.4 네이티브 모듈 빌드 (RL/테스트용)

학습 환경과 결정론 테스트는 `SimGame` 네이티브 바인딩을 사용한다.

```bash
# tetris_py .pyd/.so 를 빌드
cmake -B build-py -DTETRIS_BUILD_PY=ON -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_TEST=OFF \
      -Dpybind11_DIR=$(uv run --directory python python -m pybind11 --cmakedir)
cmake --build build-py --config Release

# 빌드된 모듈을 python/sim/ 에 복사
# (구체 위치는 Part 8 에서 다룬다)
```

학습 파이프라인 전체는 Part 8 에서 다룬다. 여기서는 "환경이 준비됐다" 까지만 목표.

### 7.5 Colab 환경

현재 Colab 통합 노트북(`python/train/train_model_zoo_colab.ipynb`)은 uv 대신
`pip`를 쓴다. Colab 런타임에는 uv가 기본 설치돼 있지 않다:

```python
!pip install -r /content/Tetris-Multiplayer-RL/python/requirements-colab.txt
```

`requirements-colab.txt` 는 lightweight `requirements.txt` 에 `pybind11`, `torch`,
`gymnasium`, `onnx`, `onnxscript` 를 추가한 것이다. 학습 프레임워크(SB3, CleanRL,
LightZero) 는 주석 처리돼 있고 사용자가 취향대로 풀면 된다.

---

## 8. 이 장 끝에 가능한 것

Part 0 체크포인트의 산출물은 `CMakeLists.txt`와 `src/main.cpp` 두 파일, 그리고
실행되는 `tetris` 뼈대 하나다. 이 장의 나머지 CMake 해부는 최종 구조를 미리
보는 지도이며, `sim_hash_dump`, relay, meta, Python 타깃은 해당 Part에서 source를
추가할 때 차례로 활성화한다.

## 이 장에서 완성된 것

- `CMakeLists.txt` 기준으로 게임, 테스트, 릴레이, 메타, pybind11 타깃이 어떤 소스 집합을 묶는지 추적했다.
- `TETRIS_BUILD_GAME`, `TETRIS_BUILD_RELAY`, `TETRIS_BUILD_META`, `TETRIS_BUILD_PY`, `TETRIS_BUILD_BOT`, `TETRIS_USE_SDL2` 가 어떤 의존성과 바이너리 구성을 바꾸는지 정리했다.
- `platform/win32.cpp`, `platform/sdl.cpp`, `meta/`, `server/`, `bindings/` 까지 현재 저장소 구조 기준으로 빌드 그래프를 고정했다.

## 수동 테스트

```bash
cmake -B build
cmake --build build
./build/tetris
```

기대 결과: configure와 compile이 성공하고 `tetris project skeleton`이 출력된다.
`sim_hash_dump`는 Part 1 완료 게이트에서 처음 추가한다.
