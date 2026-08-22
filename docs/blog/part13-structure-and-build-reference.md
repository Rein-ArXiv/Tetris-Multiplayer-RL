# Part 13: 완성 구조와 확장 레퍼런스

> **시리즈:** 제로부터 멀티플레이어 테트리스 + RL | [시리즈 목차](./README.md) | **Part 13**
>

---

## 이번 Part의 성격

앞선 열세 장과 달리 이 장은 **만드는 장이 아니라 찾아보는 장**이다. 순서대로 읽을 필요가 없고, 필요할 때 해당 절만 펼치면 된다.

- **선행 상태:** [Part 0](./part0-project-setup.md) ~ [Part 12](./part12-hardening-and-release.md) 를 따라 만든 저장소. 또는 완성된 저장소를 그대로 받은 상태.
- **이번 Part의 파일:** 없다. 새로 만드는 것이 없다.
- **연결점:** 각 장이 조금씩 확장해 온 `CMakeLists.txt` 의 최종 형태를 한자리에서 본다.
- **완료 게이트:** 없다. 대신 §7 의 확장 시나리오 중 하나를 골라 실제로 고쳐 보면 이 저장소의 계층 경계가 몸에 붙는다.

두 가지 용도를 노린다.

1. **전체 그림.** 다 만들고 나서 “그래서 이게 어떻게 하나로 묶이지”를 확인한다. 디렉터리 경계, 빌드 타깃, 의존성, 플랫폼별 구성을 조회한다.
2. **고칠 때의 지도.** "보드를 20×10 이 아니라 다르게 하고 싶다", "홀드 기능을 넣고 싶다", "새 학습 알고리즘을 붙이고 싶다" 같은 상황에서 **어느 파일을 건드려야 하고 무엇이 함께 깨지는지**를 §7 이 안내한다.

특히 두 번째가 중요하다. 이 저장소에는 컴파일러가 잡아주지 않는 계약이 몇 개 있다 — 결정론 해시, C++/Python 패리티, wire 포맷, ONNX 입출력 이름, 그리고 셰이더 정점 속성과 C++ 정점 버퍼의 대응. 이것들은 어긋나도 빌드가 성공하고, 한참 뒤에 이상한 증상으로 나타난다. §8 이 그 목록이다.

## 1. 레포 구조 한눈에

완성된 저장소의 주요 경로는 다음과 같다. 이 트리는 파일 개수를 고정하는 재고표가 아니라 책임 경계를 찾는 지도다. 세부 파일 목록은 `rg --files`와 현재 `CMakeLists.txt`를 기준으로 확인한다.

```text
Tetris-Multiplayer-RL/
├── CMakeLists.txt         ← 빌드 진입점
├── Makefile               ← cmake 호출을 감싼 얇은 편의 래퍼
├── pyproject.toml         ← Python 의존성 (uv)
├── README.md              ← 완성 상태의 실행·아키텍처 요약
│
├── core/                  ← 순수 유틸 (외부 의존 없음)
│   ├── constants.h        ← TICKS_PER_SECOND=60, SECONDS_PER_TICK
│   ├── input.h            ← INPUT_LEFT/RIGHT/DOWN/ROTATE/DROP 비트마스크
│   ├── rng.h              ← XorShift64* 결정론 RNG
│   ├── hash.h             ← FNV-1a 64-bit 상태 해시
│   └── replay.h/.cpp      ← 입력 리플레이 저장/로드
│
├── src/                   ← 게임 로직 + 렌더링 래퍼 + 진입점
│   ├── sim_game.h/.cpp    ← SimGame (헤드리스 시뮬)
│   ├── sim_grid.h         ← 20×10 보드
│   ├── sim_block.h        ← 테트로미노 상태
│   ├── sim_blocks.h       ← L/J/I/O/S/T/Z 팩토리
│   ├── game.h/.cpp        ← SimGame + 렌더링/오디오 래퍼
│   ├── gui.h/.cpp         ← 버튼·패널·모달 등 UI 위젯
│   ├── colors.h/.cpp      ← 팔레트
│   ├── position.h/.cpp    ← (row, column)
│   └── main.cpp           ← 진입점, 앱 모드 FSM, 60Hz 루프
│
├── platform/              ← 창/입력/GL 컨텍스트 백엔드 (둘 중 하나 선택)
│   ├── platform.h         ← 공용 인터페이스
│   ├── win32.cpp          ← Win32 + WGL 3.3 Core 컨텍스트 (Windows 기본)
│   ├── sdl.cpp            ← SDL2 + SDL_GL 3.3 Core 컨텍스트 (macOS/Linux 기본)
│   └── macos/Info.plist.in
│
├── renderer/              ← OpenGL 3.3 Core 2D 렌더러
│   ├── renderer.h/.cpp    ← 정점 배처, rect/rounded rect, 뷰포트·시저
│   ├── gl_api.h/.cpp      ← X-매크로 GL 함수 포인터 테이블 + 로더
│   ├── gl_shaders.h       ← GLSL 330 core 정점/조각 셰이더 (프로그램 하나)
│   ├── gl_internal.h      ← 렌더러 내부 API (glb_rect / glb_quad / glb_flush …)
│   ├── text_gl.cpp        ← stb_truetype 래스터화 + R8 글리프 아틀라스
│   ├── shake.h/.cpp       ← 화면 흔들림
│   ├── image.h            ← 이미지 핸들 인터페이스
│   └── image_gl.cpp       ← PNG decode → GL 텍스처 업로드
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
│   ├── relay.h/.cpp       ← 바이트 포워더
│   ├── match_uuid.h       ← 경기 결과 멱등성 키 생성
│   ├── player_session.h   ← ranked player 단일 활성 session lease
│   └── worker_group.h     ← 워커 스레드 수명 관리 (헤더 온리)
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
│   ├── sim_hash_dump.cpp     ← 결정론 회귀 테스트 진입점
│   └── worker_group_test.cpp ← relay 워커 수명 회귀 테스트
│
├── python/                ← Python 레이어
│   ├── requirements.txt
│   ├── requirements-colab.txt
│   ├── sim/               ← 네이티브 모듈 래퍼
│   ├── common/            ← 학습·추론 공용 (env, features, models, checkpoint)
│   ├── netbot/            ← framing/input 패리티 + ONNX export CLI
│   ├── train/             ← 학습 스크립트 + Colab 노트북
│   │   ├── ppo_tetris.py / dqn_tetris.py / cem_tetris.py
│   │   ├── cbmpi_tetris.py / muzero_tetris.py / policy_gradient_tetris.py
│   │   ├── rl_common.py
│   │   └── setup_colab.ipynb / train_model_zoo_colab.ipynb
│   ├── tests/             ← pytest 스위트
│   ├── tools/             ← 실제 TCP relay 수용량 측정 도구
│   └── legacy/            ← 이전 Pygame 구현 (비빌드, 참조용)
│
├── third_party/
│   ├── dr_mp3.h           ← 단일 헤더 MP3 디코더 (public domain)
│   ├── stb_truetype.h     ← 단일 헤더 TTF 래스터라이저 (public domain)
│   ├── stb_image.h        ← 단일 헤더 PNG/JPG 디코더 (public domain)
│   ├── httplib.h          ← cpp-httplib (게임/릴레이/메타 HTTP)
│   ├── sqlite3.{c,h}      ← SQLite amalgamation (tetris_meta)
│   ├── sqlite3ext.h       ← SQLite 확장 헤더 (amalgamation 동봉)
│   └── fetch_onnxruntime.sh
│
├── scripts/               ← 플랫폼별 배포 번들 + 운영 스크립트
│   ├── release_win.ps1
│   ├── release_macos.sh
│   ├── release_linux.sh
│   ├── release_server_linux.sh
│   └── backup_meta_db.sh
│
├── deploy/                ← Caddy / cloudflared / systemd 예시 설정
├── docs/                  ← blog/ (이 시리즈) + 설계·운영 문서
├── web/                   ← 정적 랭킹 페이지 (ranking/index.html)
├── model/                 ← bots/*.onnx 배포 슬롯 + bots.cfg.example
├── Font/                  ← NanumGothic.ttf (한글 TTF), monogram.ttf
├── Sounds/                ← music.mp3, rotate.mp3, clear.mp3
└── assets/                ← images.cfg + icons/player.png, opponent.png, bot.png
```

`Sounds/`에 드롭·가비지 전용 효과음 파일(`drop.mp3`, `garbage.mp3`)이 있으면 각각 로드하고, 없으면 무음으로 두지 않고 **재생 시점에 대체음을 낸다** — 드롭은 `rotate.mp3`, 가비지는 `clear.mp3` 로 폴백해 조작 피드백을 유지한다(`src/game.cpp` 의 `SubmitInput`/`Tick`). 핸들을 alias 하는 것이 아니라 재생하는 순간 대체 핸들을 고르는 방식이라, 소멸자에서 같은 핸들을 두 번 해제할 일이 없다. BGM/SFX 하나의 로드 실패가 전체 오디오 초기화나 게임 실행을 막지 않으며, 성공한 핸들만 `Game` 소멸 시 해제한다.

한 줄 책임 정리:

| 디렉토리 | 책임 | 외부 의존 |
|---|---|---|
| `core/` | 순수 C++ 헬퍼(RNG·해시·상수·입력 비트마스크·리플레이) | 없음 |
| `src/` | 테트리스 로직 + 렌더링 래퍼 + UI + 진입점 | `core/`, `renderer/`, `net/` |
| `platform/` | OS 창/입력/GL 컨텍스트 추상화 (`platform.h` 공용 계약과 플랫폼별 구현) | Win32 API + WGL 또는 SDL2 |
| `renderer/` | OpenGL 3.3 Core 2D (사각형·텍스트·이미지·셰이크) | OpenGL 3.3 Core 드라이버, `stb_truetype`, `platform/` |
| `audio/` | MP3 로드 + 재생 (공용 헤더와 플랫폼별 백엔드) | XAudio2 또는 SDL2_audio, `third_party/dr_mp3.h` |
| `net/` | TCP 소켓 → 메시지 프레이밍 → lockstep 세션 | WinSock2 또는 BSD 소켓 + pthread |
| `server/` | `tetris_relay` 바이너리: 매치메이킹 + 바이트 릴레이 | `net/` + `meta/http_client.cpp` + `third_party/httplib.h` |
| `meta/` | `tetris_meta` 바이너리: HTTP+SQLite 메타/랭킹 + 게임·릴레이용 HTTP 클라이언트 | `third_party/sqlite3.c`, `third_party/httplib.h` |
| `bot/` | `Ort::Session` 로 학습된 정책 추론 | ONNX Runtime (옵션) |
| `bindings/` | `SimGame` 을 pybind11 모듈 `tetris_py` 로 노출 | pybind11 |
| `python/` | 학습·export, framing/placement 패리티, pytest | 기본: numpy, pytest, pybind11 / 학습·export: torch, gymnasium, onnx, onnxscript |
| `third_party/` | 벤더링된 단일 헤더 + 외부 바이너리 설치 스크립트 | — |
| `scripts/` | 플랫폼별 배포 번들 빌더 + 운영 백업 스크립트 | — |
| `web/` | 정적 랭킹 웹 페이지 — `tetris_meta` 의 leaderboard API(`/v1/leaderboard`)를 same-origin 으로 조회 | 브라우저 + 리버스 프록시 라우팅 (`deploy/`) |
| `docs/` | 블로그 및 설계 문서 | — |

의존성은 하위 계층에서 UI로 역류하지 않게 유지한다. `core/`는 플랫폼을 모르고, `server/`는 `net/`과 meta HTTP 클라이언트를 쓰되 `src/`의 게임·화면 코드를 링크하지 않는다. relay는 입장·룸·ranked summary 같은 제어 프레임은 해석하지만 게임 시뮬레이션 상태를 만들지 않는다. `python/`의 학습 경로는 `bindings/`를 거쳐 `SimGame`에 닿고, wire 테스트 도구만 framing 규약을 별도로 미러링한다. `web/`은 어떤 빌드 타깃에도 들어가지 않는다 — 정적 HTML 하나가 리버스 프록시를 통해 meta 서버와 같은 origin 에서 서빙되며, 그래서 CORS 설정 없이 leaderboard API 를 그대로 호출할 수 있다.

---

## 2. 의존성 총정리

각 의존성이 **언제 필요하고**, **어떻게 확보**하며, **어느 타깃에 링크**되는지 한 표로 정리한다.

### 2.1 플랫폼 내장 (설치 불필요)

| 라이브러리 | 플랫폼 | 링크 이름 | 쓰임 |
|---|---|---|---|
| OpenGL | Windows | `opengl32` | GL 컨텍스트 생성(WGL) + GL 1.1 심볼 |
| OpenGL | Linux/macOS | `OpenGL::GL` | 같음 (`find_package(OpenGL REQUIRED)`) |
| GDI / GDI+ | Windows | `gdi32`, `gdiplus` | 픽셀 포맷 설정·`SwapBuffers`(gdi32), PNG 디코딩(gdiplus) |
| WinMM | Windows | `winmm` | `timeBeginPeriod` 로 고해상도 타이머 |
| WinSock2 | Windows | `ws2_32` | TCP 소켓 |
| XAudio2 | Windows | `xaudio2`, `ole32` | 오디오 재생 |
| pthread | Linux/macOS | `Threads::Threads` | `std::thread` 런타임 |

Windows 에서는 Visual Studio 를 설치하면 위 항목이 SDK 에 들어 있다 — `opengl32.lib` 도 포함이라 GL 을 위해 따로 받을 것이 없다. macOS 도 Xcode command-line tools 에 OpenGL 프레임워크가 들어 있다. **Linux 만 별도 개발 패키지가 필요하다** — Debian/Ubuntu 는 `libgl1-mesa-dev`, Fedora 는 `mesa-libGL-devel` 이다. 설치 절차는 [Part 0](./part0-project-setup.md) 에 있다.

링크되는 것은 GL 라이브러리뿐이고, 3.3 Core 의 함수는 링커가 아니라 **런타임에 함수 포인터로 받는다**(`renderer/gl_api.cpp`). Windows 의 `opengl32.dll` 이 GL 1.1 까지만 export 하기 때문인데, 플랫폼마다 다른 코드를 두지 않으려고 세 플랫폼 모두 같은 조회 경로(`platform_gl_get_proc`)를 탄다. 그래서 별도의 GL 로더 라이브러리 의존성이 없다.

런타임 요구사항도 하나 늘었다. 클라이언트를 실행하는 기계의 드라이버가 **OpenGL 3.3 Core 프로파일**을 줘야 한다. 못 주면 `gl_load_functions()` 가 빠진 진입점 이름을 전부 나열하고 실패한다. 이때 `renderer_init` 은 `false` 를 반환하고, 호출자(`src/main.cpp`)가 `platform_fatal_error` 로 네이티브 메시지박스를 띄운 뒤 정리하고 종료한다 — 상세 원인(빠진 진입점 이름)은 여전히 stderr 에만 남는다. 서버 타깃(`tetris_relay`, `tetris_meta`)과 테스트 타깃은 GL 을 링크하지도, 요구하지도 않는다.

### 2.2 외부 라이브러리

**SDL2** — macOS/Linux 의 창·입력·GL 컨텍스트, 그리고 해당 경로의 오디오.

- Windows: 기본 비활성 (Win32 경로 사용). `-DTETRIS_USE_SDL2=ON` 으로 활성화 시 `-DSDL2_DIR=...` 로 위치 지정.
- macOS: `brew install sdl2`
- Linux: `apt install libsdl2-dev`
- CMake 에서는 `find_package(SDL2 REQUIRED)` 로 탐색. 배포 버전에 따라 `SDL2::SDL2` 타깃이 있을 수도 있고 `${SDL2_LIBRARIES}` 변수만 제공할 수도 있어, CMakeLists 는 두 경로를 모두 지원한다.

**pybind11** — `tetris_py` 네이티브 모듈 빌드에만 필요.

- `pip install pybind11` 로 설치. 저장소에는 서브모듈로 벤더링돼 있지 않다.
- CMake 에서 `find_package(pybind11 CONFIG QUIET)` 로 탐색. 없으면 `FATAL_ERROR` 로 "`-Dpybind11_DIR=$(python -m pybind11 --cmakedir)` 를 넘겨라" 는 힌트를 준다.
- CMake 4.0+ 는 `FindPythonInterp` / `FindPythonLibs` 가 삭제됐으므로, `set(PYBIND11_FINDPYTHON ON)` 으로 모던 `FindPython` 을 사용하도록 힌트.

**OpenSSL** — `TETRIS_ENABLE_HTTPS=ON`(기본값) 이고 시스템에 있으면 자동으로 붙는다. 없으면 경고만 내고 빌드는 계속되며, 런타임에 `https://` 메타 URL 이 거부된다.

**ONNX Runtime** — 봇(`Single vs Bot`)의 CPU 추론 전용. 용량 때문에 git 서브모듈 대신 `third_party/fetch_onnxruntime.sh`로 다운로드하며, CMake는 벤더링된 헤더와 라이브러리를 명시적으로 검사한다.

- 공식 GitHub release 에서 CPU 빌드만 벤더링: Windows `.zip`, macOS `.tgz`(universal2), Linux `.tgz`(x64 또는 aarch64).
- `third_party/onnxruntime/include/onnxruntime_cxx_api.h` 가 있어야 `TETRIS_BUILD_BOT=ON` 이 성공.

**dr_mp3** — 단일 헤더 MP3 디코더 (public domain). `third_party/dr_mp3.h` 로 **이미 저장소에 벤더링**돼 있다. `audio/audio.cpp` 와 `audio/sdl_audio.cpp` 양쪽에서 사용한다.

**stb 계열** — `third_party/stb_truetype.h` 와 `third_party/stb_image.h` 가 **저장소에 벤더링**돼 있다 (둘 다 public domain 단일 헤더). `renderer/text_gl.cpp` 가 모든 플랫폼에서 `stb_truetype` 로 TTF 를 CPU coverage bitmap 으로 래스터화한 뒤, 그 비트맵을 R8 글리프 아틀라스 텍스처에 올린다 — **글자 모양을 만드는 일은 여전히 CPU 가 한다.** GPU 에는 TTF 아웃라인을 래스터화하는 기능이 없기 때문이고, GL 로 옮기면서 바뀐 것은 그 비트맵을 두는 곳이다. `renderer/image_gl.cpp` 의 비-Win32 분기는 `stb_image` 로 PNG/JPG 를 디코딩하고 결과를 GL 텍스처로 업로드한다 (Windows 는 GDI+ 사용 — 이미지 디코딩 전용이며 텍스트에는 쓰지 않는다). 각 헤더는 정확히 한 번역 단위에서 `STB_TRUETYPE_IMPLEMENTATION` / `STB_IMAGE_IMPLEMENTATION` 매크로와 함께 include 된다.

**cpp-httplib / SQLite amalgamation** — `third_party/httplib.h`, `third_party/sqlite3.{c,h}`. 전자는 게임 클라이언트까지 포함한 세 바이너리가 모두 쓰고, 후자는 `tetris_meta` 전용이다. 둘 다 존재 검사를 통과하지 못하면 CMake 가 즉시 `FATAL_ERROR` 로 멈춘다.

### 2.3 Python 환경

저장소 루트에는 `pyproject.toml` 이 있다. 로컬 기본 환경은 가볍게 유지하고, PyTorch/Gymnasium/ONNX 는 학습·export extra 로만 설치한다. 저사양 배포 머신에서 torch 를 끌어오지 않기 위해서다.

```text
pyproject.toml                 → numpy 기본, pytest/pybind11 dev, torch/gymnasium/onnx/onnxscript extra
python/requirements.txt        → pip fallback: numpy, pytest
python/requirements-colab.txt  → requirements.txt + pybind11 + torch + gymnasium + onnx + onnxscript
```

루트 `pyproject.toml` 의 핵심은 다음이다.

**현재 소스 발췌 — `pyproject.toml`**

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

`requires-python` 은 `>=3.12` 다. uv 설치와 `uv sync --dev` 는 [Part 0](./part0-project-setup.md) 에 있다. 여기서 짚을 것은 **왜 이렇게 계층을 나눴는가** 다.

기본 의존성이 numpy 하나뿐이라는 점이 핵심이다. 배포 서버(meta/relay 를 돌리는 작은 머신)에서 `uv sync --dev` 를 하면 torch 가 딸려오지 않는다. torch 는 수 GB 이고 그 머신에서는 학습을 하지 않으므로 받을 이유가 없다.

학습이나 export 를 실제로 할 때만 extra 를 켠다. 대개 Colab 이다.

```bash
uv sync --dev --extra train --extra export
```

`train` 과 `export` 를 나눈 것도 같은 이유다. 학습만 할 거면 `onnx`/`onnxscript` 가 필요 없고, 이미 있는 `.pt` 를 변환만 할 거면 `gymnasium` 이 필요 없다. `requirements-colab.txt` 는 이 둘을 합친 pip 용 폴백이다 — Colab 런타임에는 uv 가 기본으로 없기 때문에 남겨 뒀다.

### 2.4 타깃별 의존성 매트릭스

| 타깃 | OpenGL | SDL2 | Win32 API | ONNX RT | pybind11 | 필요 조건 |
|---|---|---|---|---|---|---|
| `tetris` (Win32 경로) | `opengl32` | — | ✓ | 옵션 | — | Windows only, `httplib.h` 필수, 런타임에 GL 3.3 Core |
| `tetris` (SDL2 경로) | `OpenGL::GL` | ✓ | — | 옵션 | — | 전 플랫폼, `httplib.h` 필수, 런타임에 GL 3.3 Core |
| `tetris_relay` | — | — | ws2_32만 | — | — | 헤드리스, Termux OK, `httplib.h` 필수 |
| `tetris_meta` | — | — | ws2_32만 | — | — | `sqlite3.{c,h}` + `httplib.h` 필수 |
| `sim_hash_dump` | — | — | — | — | — | 결정론 테스트, 전 플랫폼, 의존성 0 |
| `worker_group_test` | — | — | — | — | — | Threads 만 (비-Windows) |
| `tetris_py` (pybind11) | — | — | — | — | ✓ | Colab/로컬 Python |

이 표가 CMakeLists 의 옵션 플래그 설계를 결정한다. 특히 마지막 두 줄이 중요하다 — 시뮬레이션 코어는 **아무 것에도 의존하지 않기 때문에** 어떤 환경에서든 빌드되고, 그래서 결정론 게이트를 CI 에서 항상 돌릴 수 있다.

---

## 3. CMakeLists.txt 해부

이 절에서는 최종 `CMakeLists.txt`를 책임별 블록으로 발췌해 설명한다. 줄 번호는 사용하지 않고 타깃·옵션·변수 이름으로 현재 소스를 찾는다. 각 Part의 `CMakeLists 확장`은 그 기능이 처음 빌드 가능한 체크포인트를 보여주며, 이 장은 완성된 타깃 관계를 조회하는 레퍼런스다.

| Part | 추가되는 소스 | 그 시점에 빌드 가능한 타깃 |
|---|---|---|
| 0 | `src/main.cpp`(스텁) | 자작 `tetris` 스텁 |
| 1 | `src/sim_game.cpp`, `src/position.cpp`, 관련 시뮬 헤더, `tests/sim_hash_dump.cpp` | `sim_hash_dump` |
| 2 | `platform/platform.h`, `platform/win32.cpp` 또는 `platform/sdl.cpp` | Part 2 체크포인트 데모 |
| 3 | `renderer/renderer.cpp`, `renderer/gl_api.cpp`, `renderer/text_gl.cpp`, `renderer/image_gl.cpp`, `renderer/shake.cpp`, `src/gui.cpp`, `src/colors.cpp` | Part 3 체크포인트 데모 |
| 4 | `src/game.cpp`, `src/main.cpp`(본체), `core/replay.cpp` | 싱글플레이 `tetris` |
| 5 | `audio/audio.cpp` 또는 `audio/sdl_audio.cpp` | 소리 나는 `tetris` |
| 6 | `net/socket.cpp`, `net/framing.cpp`, `net/session.cpp` | 직결 멀티 `tetris` |
| 7 | `server/*.cpp`, `server/worker_group.h` | `tetris_relay`, `worker_group_test` |
| 8 | `bindings/tetris_py.cpp` | `tetris_py` |
| 9 | `bot/placement.cpp`, `bot/bot_onnx.cpp` | 봇 포함 `tetris` |
| 10 | `meta/*.cpp`, `third_party/sqlite3.c`, `meta/http_client.cpp` | `tetris_meta` |
| 11 | (신규 파일 없음 — 기존 파일 확장) | — |
| 12 | (신규 파일 없음 — 옵션·패키징) | 전체 |

### 3.1 프롤로그

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.15)
# C 언어도 활성화 — third_party/sqlite3.c (amalgamation) 를 빌드하려면 필요.
# tetris_meta 타겟만 C 를 쓰지만 enable_language 는 프로젝트 루트에서 선언해야 한다.
project(tetris CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: UTF-8 소스 파일 인코딩 (한국어 주석 깨짐 방지)
if (MSVC)
    add_compile_options(/utf-8)
endif()
```

CMake 3.15 는 `find_package` 의 `CONFIG` 모드, `target_link_libraries` 의 타깃 기반 의존성 같은 현대적 기능을 안정적으로 지원하는 최저선이다. C++17 은 `std::optional`, structured binding, `if constexpr` 를 쓰기 위해 필수.

`project(tetris CXX C)` 의 `C` 는 주석이 설명하듯 `tetris_meta` 타깃만을 위한 것이다. 그런데 `enable_language` 계열 선언은 프로젝트 루트에서 해야 하므로, `TETRIS_BUILD_META=OFF` 인 대부분의 빌드에서도 C 컴파일러를 찾는다. C 컴파일러가 없는 희귀한 환경에서는 이 줄이 첫 실패 지점이 된다.

MSVC 의 `/utf-8` 는 소스/실행 인코딩 모두 UTF-8 로 설정하는 플래그다. 이 저장소는 C++ 주석이 한국어로 많이 적혀 있고, MSVC 가 기본으로 가정하는 시스템 로케일(CP949 등)에서 컴파일하면 `warning C4819` 가 쏟아진다. `/utf-8` 하나로 전부 해결.

### 3.2 옵션 플래그와 캐시 변수

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Build options
#
#   TETRIS_BUILD_GAME  — Game executable. OpenGL 3.3 Core renderer with a thin
#                         Win32 or SDL2 window/context backend.
#   TETRIS_BUILD_PY    — pybind11 module (tetris_py) wrapping SimGame. Portable.
#   TETRIS_BUILD_TEST  — SimGame determinism regression (sim_hash_dump). Portable.
#   TETRIS_BUILD_RELAY — Headless matchmaking/relay server (server/*.cpp).
#                         No GUI/audio. Linux(Termux), macOS, Windows 모두 OK.
# -----------------------------------------------------------------------------
option(TETRIS_BUILD_GAME  "Build the handmade game executable"              ON)
option(TETRIS_BUILD_PY    "Build the pybind11 module (tetris_py)"           OFF)
option(TETRIS_BUILD_TEST  "Build the SimGame determinism test"              ON)
option(TETRIS_BUILD_RELAY "Build the tetris_relay matchmaking server"       OFF)
# TETRIS_BUILD_META — HTTP + SQLite metadata server (guest/auth/matches/leaderboard).
# Typically deployed separately so the relay owns no durable database state.
option(TETRIS_BUILD_META  "Build the tetris_meta HTTP+SQLite metadata server" OFF)
# TETRIS_BUILD_BOT — Section C: link onnxruntime and compile bot/*.cpp.
# OFF 이면 bot_onnx 가 "not vendored" 스텁으로 빌드되어 ONNX 모델 로드는
# 실패한다. Single vs Bot과 내장 휴리스틱 봇은 그대로 사용할 수 있다.
option(TETRIS_BUILD_BOT   "Link onnxruntime (Section C bot inference)"      OFF)
option(TETRIS_ENABLE_HTTPS "Enable HTTPS for tetris_meta clients when OpenSSL is available" ON)
option(TETRIS_ENABLE_DEBUG_UI "Enable in-game debug overlays in the game client" OFF)
option(TETRIS_ENABLE_NET_TRACE "Enable verbose game-client net/session trace logs" OFF)
set(TETRIS_DEFAULT_RELAY_ENDPOINT "127.0.0.1:7777" CACHE STRING
    "Default relay endpoint embedded in the game client menu")
set(TETRIS_DEFAULT_META_URL "" CACHE STRING
    "Default tetris_meta base URL embedded in the game client")
```

각 빌드 토글의 의미:

- **`TETRIS_BUILD_GAME`** — 게임 실행 파일(`tetris`). 기본 ON. Windows 에서는 Win32 경로, 그 외는 SDL2 경로로 빌드된다. **기본값이 ON 이라는 점이 중요하다** — 아직 `src/game.cpp` 나 `renderer/*.cpp` 가 없는 Part 1~3 시점에는 반드시 `-DTETRIS_BUILD_GAME=OFF` 를 명시해야 configure 가 통과한다.
- **`TETRIS_BUILD_PY`** — pybind11 모듈(`tetris_py`). 기본 OFF — Colab 학습 환경이나 native Sim 테스트에 사용한다. 배포 클라이언트와 인게임 ONNX 봇에는 필요 없다.
- **`TETRIS_BUILD_TEST`** — 회귀 테스트. 기본 ON — GUI 가 없으므로 어느 플랫폼에서든 빌드된다. `sim_hash_dump` 와 `worker_group_test` 를 만든다.
- **`TETRIS_BUILD_RELAY`** — `tetris_relay` 매치메이킹 서버. 기본 OFF — 릴레이 호스트에서만 켠다.
- **`TETRIS_BUILD_META`** — `tetris_meta` HTTP+SQLite 메타/RP 서버. 기본 OFF — 별도 호스트에서만 켠다.
- **`TETRIS_BUILD_BOT`** — ONNX Runtime 링크. OFF 라도 `bot/bot_onnx.cpp` 는 컴파일되지만 `TETRIS_HAS_ONNXRUNTIME` 매크로가 미정의라 **스텁 모드**로 빌드돼, ONNX 모델에 대한 `Load()` 가 실패한다. 다만 "Single vs Bot" 자체는 내장 휴리스틱 봇으로 항상 열 수 있고, 학습 모델만 ONNX Runtime 이 있을 때 추가로 선택 가능하다.

기능 토글 셋:

- **`TETRIS_ENABLE_HTTPS`** — 기본 **ON**. 켜져 있고 OpenSSL 이 발견되면 메타 클라이언트가 `https://` URL 을 쓸 수 있다 (`CPPHTTPLIB_OPENSSL_SUPPORT` + `OpenSSL::SSL`/`OpenSSL::Crypto` 링크). 즉 이 옵션은 **OpenSSL 을 끌어올 수 있다**. OpenSSL 이 없으면 경고만 내고 런타임에 `https` URL 을 거부한다.
- **`TETRIS_ENABLE_DEBUG_UI`** — 기본 OFF. 게임 클라이언트의 인게임 디버그 오버레이를 켠다 (`TETRIS_ENABLE_DEBUG_UI=1` 매크로). 해시 덤프 단축키 `H` 와 봇 속도 조절 단축키는 **이 빌드에서만 존재한다**.
- **`TETRIS_ENABLE_NET_TRACE`** — 기본 OFF. net/session 의 상세 추적 로그를 켠다.

마지막 두 줄의 `set(... CACHE STRING ...)` 은 옵션이 아니라 **문자열 캐시 변수**다. 이 둘이 릴리스 바이너리에 박히는 기본 릴레이 주소와 메타 서버 주소를 결정하므로, 배포를 재현하려면 반드시 알아야 한다. 개인 IP 를 소스에 하드코딩하지 않기 위한 장치이기도 하다.

```bash
cmake -S . -B build \
      -DTETRIS_DEFAULT_RELAY_ENDPOINT=relay.example.com:7777 \
      -DTETRIS_DEFAULT_META_URL=https://meta.example.com
```

이 값들은 컴파일 타임 매크로로 주입되고(아래 `target_compile_definitions` 참조), 런타임에 환경변수와 CLI 인자가 차례로 덮어쓴다. `src/main.cpp` 의 인자 파싱은 모든 초기화보다 앞에 있어서, 잘못된 인자는 창·GL 컨텍스트·소켓 같은 자원이 하나도 생기기 전에 종료 코드 2 로 끝난다. 읽는 우선순위는 다음과 같다:

| 우선순위 | 릴레이 주소 | 메타 URL | 비고 |
|---|---|---|---|
| 1 (최저) | CMake `TETRIS_DEFAULT_RELAY_ENDPOINT` (기본 `127.0.0.1:7777`) | CMake `TETRIS_DEFAULT_META_URL` (기본 빈 문자열) | 바이너리에 박힌 값 |
| 2 | 환경변수 `TETRIS_RELAY_ENDPOINT` | 환경변수 `TETRIS_META_URL` | 파싱 실패 시 경고 후 이전 값 유지 |
| 3 (최고) | CLI `--relay host[:port]` (메뉴의 Matchmaking·Custom Room 이 쓰는 릴레이 주소) | CLI `--meta URL` | 그 실행에만 적용 |

릴레이 주소를 덮어쓰는 플래그는 `--relay` 하나다. 나머지 네트워크 플래그는 이 우선순위 사슬 밖의 별도 경로다 — `--queue host[:port]` 는 메뉴를 거치지 않고 그 주소의 매치메이킹 큐로 즉시 입장하는 자체 endpoint 를 가지며, `--host PORT` 와 `--connect host[:port]` 는 릴레이를 아예 거치지 않는 직결 P2P 전용이라 릴레이 주소와 무관하다. `--host` 는 대기 포트 하나만 받는다 — 여기에 주소를 넘기면 포트 파싱 오류로 즉시 종료되고, 포트만 넘기면 릴레이 주소는 그대로 둔 채 직결 호스트 모드로 들어간다.

빈 문자열 메타 URL은 "메타 서버 없음"을 뜻한다. 게스트 토큰 발급, 랭킹·아이콘 조회, ranked 결과 저장이 비활성화되고 게임은 로컬·직결·unranked relay 모드로 동작한다. relay에 `--meta`를 주지 않은 경우도 같은 `player_id=0` 계약을 쓴다.

바로 아래에서 OpenSSL 을 찾는다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
if (TETRIS_ENABLE_HTTPS)
    find_package(OpenSSL QUIET)
    if (OpenSSL_FOUND)
        message(STATUS "OpenSSL found: HTTPS meta client enabled")
    else()
        message(WARNING "OpenSSL not found: https:// meta URLs will be rejected at runtime")
    endif()
endif()
```

`QUIET` + 경고 조합이 핵심이다. HTTPS 는 "있으면 좋은" 기능이라 없다고 빌드를 멈추지 않는다. `OpenSSL_FOUND` 변수는 뒤의 세 타깃(`tetris`, `tetris_relay`)에서 다시 참조된다.

플랫폼 백엔드 기본값:

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# TETRIS_USE_SDL2 — Use SDL2 for the cross-platform window/input/GL context and audio backend.
# Text and images still go through the shared OpenGL renderer.
# Default ON on non-Windows so macOS/Linux users get it automatically.
# On Windows, default OFF to preserve the handmade Win32 window/audio path.
if (WIN32)
    option(TETRIS_USE_SDL2 "Use SDL2 backend (cross-platform)" OFF)
else()
    option(TETRIS_USE_SDL2 "Use SDL2 backend (cross-platform)" ON)
endif()
```

`TETRIS_USE_SDL2` 는 창·GL 컨텍스트 생성과 오디오만 바꾼다. GL 렌더러와 셰이더, 텍스트 래스터화는 어느 쪽에서도 공통이다 — 두 백엔드가 같은 3.3 Core 프로파일을 요청하기 때문에 `#version 330 core` 셰이더 한 벌이 세 플랫폼에서 그대로 통한다.

### 3.3 공유 소스 목록

모든 타깃이 쓰는 순수 시뮬 파일들을 변수로 뽑아 둔다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Sources shared between all targets
# -----------------------------------------------------------------------------
# Pure simulation logic (no renderer/platform deps) — used by game, pybind11 module, and tests.
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

이 변수에는 역사가 있다 — 이 저장소의 초기 버전은 기성 게임 프레임워크 위에서 시뮬레이션과 렌더링이 붙어 있었고, 그 결합을 떼어내 "렌더러·플랫폼 의존이 없는 순수 시뮬" 만 남긴 리팩터링의 결과가 이 목록이다. 이 두 변수는 `tetris`, `tetris_py`, `sim_hash_dump` 타깃이 공유한다.

`sim_grid.h` / `sim_block.h` / `sim_blocks.h`가 헤더만 있는 이유는 이들이 구조체와 inline 멤버 함수를 담기 때문이다. 반면 `SimGame`은 `.cpp`로 분리해 게임 규칙 구현(락, 라인 클리어, 점수, 가비지, 배치 열거)이 공개 인터페이스를 포함하는 모든 번역 단위에서 반복 컴파일되지 않게 한다. RNG와 해시 구현은 여기 없다. 재사용되는 작은 inline 계약이라 `core/rng.h`와 `core/hash.h`에 헤더 전용으로 둔다.

`src/position.cpp`는 단순한 생성자만 담고 있어 헤더 inline으로 합칠 수도 있지만, 원본 구조를 계승해 별도 번역 단위로 남아 있다. 새 동작을 여기에 더하기보다 좌표 값 타입이라는 작은 책임을 유지하는 편이 낫다.

### 3.4 타깃 1 — `tetris` (게임 클라이언트)

이 섹션은 `TETRIS_BUILD_GAME=ON` 일 때만 활성화된다. 내부는 (a)~(f) 블록으로 나눠 본다.

(a) **의존성 선검사 + 공통 소스 묶음**:

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Target: tetris (handmade OpenGL 3.3 Core game client)
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_GAME)
    # 공통: 시뮬레이션 + 게임 로직 + 렌더러 공통 부분 + 네트워킹 + 봇
    #   bot/*.cpp 는 TETRIS_BUILD_BOT 과 관계없이 항상 컴파일한다 — OFF 일 때
    #   bot_onnx.cpp 는 자동으로 스텁 모드(TETRIS_HAS_ONNXRUNTIME 미정의)로
    #   빌드되어 main.cpp 의 호출부만 정상 링크된다.
    # meta/http_client.cpp 는 tetris_meta 서버와의 HTTP 통신 (guest 토큰 발급용).
    # third_party/httplib.h 가 있어야 한다 — 없으면 빌드 실패로 빠르게 감지.
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_GAME=ON 이지만 third_party/httplib.h 가 없습니다. "
            "tetris_meta 서버 호출 (guest 토큰) 용. 다운로드 후 재시도.")
    endif()
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
        renderer/gl_api.cpp
        renderer/text_gl.cpp
        renderer/shake.cpp
        renderer/image_gl.cpp
        bot/placement.cpp
        bot/bot_onnx.cpp
        meta/http_client.cpp
    )
```

주목할 점: `bot/bot_onnx.cpp` 는 `TETRIS_BUILD_BOT=OFF` 라도 **항상 컴파일된다**. ONNX 모델을 고르면 런타임에 `Load()` 가 실패하지만, 내장 휴리스틱 봇은 이 파일과 무관하게 동작한다. 이 덕분에 `main.cpp` 의 `#ifdef` 분기가 필요 없다 — 호출 쪽 코드는 항상 동일하고, 빌드 옵션은 "학습 모델을 ONNX Runtime 으로 로드할 수 있는가" 만 바꾼다.

그리고 `httplib.h` 존재 검사가 **게임 타깃 안에** 있다는 점이 중요하다. `meta/http_client.cpp` 가 게임 공통 소스이므로, 기본 게임 빌드에도 이 헤더가 반드시 있어야 한다. 즉 `httplib.h` 는 릴레이/메타 전용이 아니라 **기본 게임 빌드의 선행 조건**이다.

(b) **헤더 목록**. CMake 는 헤더를 컴파일하지 않지만, `add_executable` 에 나열해두면 IDE(Visual Studio 솔루션, Xcode 프로젝트)의 파일 트리에 나타나고 일부 제너레이터가 의존성 추적에 활용한다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    set(TETRIS_GAME_HEADERS
        ${TETRIS_SIM_HEADERS}
        src/game.h
        src/colors.h
        core/replay.h
        net/socket.h
        net/framing.h
        net/session.h
        platform/platform.h
        renderer/renderer.h
        renderer/gl_api.h
        renderer/gl_internal.h
        renderer/gl_shaders.h
        renderer/shake.h
        renderer/image.h
        audio/audio.h
        bot/placement.h
        bot/bot_onnx.h
    )
```

`${TETRIS_SIM_HEADERS}`를 앞에 펼쳐 넣으므로 시뮬 공개 헤더도 함께 나열된다. `src/gui.h`와 `meta/http_client.h`는 이 목록에 없다. 헤더 누락은 컴파일 자체에는 영향을 주지 않지만 일부 IDE의 타깃 트리 표시에는 영향을 줄 수 있다.

GL 헤더 셋(`gl_api.h` / `gl_internal.h` / `gl_shaders.h`)이 여기 나열돼 있다는 것은 이 셋이 **렌더러 내부 전용**이라는 뜻이기도 하다. `src/` 나 `game.cpp` 는 `renderer/renderer.h` 만 include 하고 GL 타입을 한 번도 보지 않는다. 셰이더 문자열조차 `gl_shaders.h` 안의 raw string literal 이라, 별도 애셋 파일이나 로딩 경로가 없다.

(c) **백엔드 분기** — GL 렌더러와 텍스트는 공통이고, `TETRIS_USE_SDL2`에 따라 창/컨텍스트와 오디오 구현을 교체한다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    if (TETRIS_USE_SDL2)
        # SDL2 경로: Mac/Linux (+ 옵션으로 Windows). 통합 백엔드 하나.
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

        # OpenGL 3.3 Core 렌더러. 함수 포인터는 런타임에 받지만 컨텍스트를
        # 만드는 진입점(SDL 경유)과 GL 1.1 심볼 때문에 GL 라이브러리는 링크한다.
        find_package(OpenGL REQUIRED)
        target_link_libraries(tetris PRIVATE OpenGL::GL)

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
            target_link_libraries(tetris PRIVATE opengl32 gdi32 gdiplus winmm ws2_32 xaudio2 ole32)
        else()
            message(FATAL_ERROR "Handmade Win32 backend is Windows-only. Set -DTETRIS_USE_SDL2=ON.")
        endif()
    endif()
```

두 분기의 **짝** 관계를 정리하면:

| 백엔드 | `platform/` | GL 컨텍스트 | `renderer/` | `audio/` |
|--------|-------------|-------------|------------|----------|
| Win32  | `win32.cpp` | WGL 2단계 (`wglCreateContextAttribsARB`) | 공통 GL 렌더러 | `audio.cpp` |
| SDL2   | `sdl.cpp`   | `SDL_GL_CreateContext` + 프로파일 속성 | 같은 공통 GL 렌더러 | `sdl_audio.cpp` |

셰이더, 정점 배처, 글리프 아틀라스, 이미지 텍스처 업로드 코드는 두 경로에서 완전히 동일하다. 백엔드가 다르게 하는 일은 "3.3 Core 컨텍스트를 만들어 current 로 만들고, `platform_gl_get_proc` 로 함수 주소를 넘겨주고, `platform_present()` 에서 버퍼를 교체한다" 세 가지뿐이다.

헤더 `platform/platform.h`, `renderer/renderer.h`, `audio/audio.h` 는 양쪽이 동일한 인터페이스를 구현한다. 그래서 `src/main.cpp`, `src/game.cpp` 는 **한 줄도 바뀌지 않는다** — 선택은 전적으로 CMake 레벨.

**두 경로 모두 GL 라이브러리를 링크한다는 점이 중요하다.** SDL2 경로는 `find_package(OpenGL REQUIRED)` 로 찾아 `OpenGL::GL` 을 걸고, Win32 경로는 링크 목록 맨 앞에 `opengl32` 을 둔다. 3.3 함수는 어차피 런타임에 함수 포인터로 받는데도 링크가 필요한 이유는 두 가지다 — 컨텍스트를 만드는 진입점(`wglCreateContext` 등)이 그 라이브러리에 있고, `glEnable` 같은 GL 1.1 심볼도 거기 있기 때문이다.

`find_package(OpenGL REQUIRED)` 의 `REQUIRED` 는 의도적이다. GL 없이는 게임이 화면을 만들 수 없으므로 configure 단계에서 즉시 멈추는 편이 낫다. Linux 에서 `libgl1-mesa-dev` 를 깔지 않았다면 여기서 걸리고, 에러 메시지가 무엇이 없는지 바로 알려준다. `OpenGL::GL` 은 CMake 내장 `FindOpenGL` 이 만드는 IMPORTED 타깃으로, Linux 에서는 `libGL.so`, macOS 에서는 `OpenGL.framework`, Windows 에서는 `opengl32.lib` 로 각각 확장된다 — 플랫폼 분기를 CMake 가 대신 해준다.

Win32 경로의 링크 목록을 한 줄씩 훑어보자:

- `opengl32` — WGL 컨텍스트 생성(`wglCreateContext`, `wglMakeCurrent`, `wglGetProcAddress`)과 GL 1.1 심볼. 3.3 함수는 여기 없고 런타임에 받는다.
- `gdi32` — `ChoosePixelFormat` / `SetPixelFormat` / `SwapBuffers`. GL 컨텍스트를 붙이기 전에 DC 의 픽셀 포맷을 정하는 데 필요하다.
- `gdiplus` — `Gdiplus::Bitmap` PNG 로더 (`renderer/image_gl.cpp`). **이미지 디코딩 전용**이며 텍스트 렌더링에는 관여하지 않는다.
- `winmm` — `timeBeginPeriod(1)` 로 `Sleep` 해상도 1ms 강제. 60 FPS 페이싱 정확도에 직결된다.
- `ws2_32` — WinSock2 (`socket`, `connect`, `send`, `recv`).
- `xaudio2` — XAudio2 상위 인터페이스.
- `ole32` — `CoInitializeEx` (XAudio2 가 COM 위에 있음).

SDL2 경로의 Windows 분기가 `gdiplus ws2_32` 둘뿐인 것은 GL 을 안 쓰기 때문이 아니라 **`OpenGL::GL` 이 바로 위에서 이미 걸렸기** 때문이다. 그 분기에 남은 것은 PNG 디코딩과 소켓뿐이다.

SDL2 경로 Linux 분기에서 `find_package(Threads REQUIRED)` 이 필요한 이유: `std::thread` 는 C++ 표준이지만 GCC/libstdc++ 는 내부적으로 pthread 를 호출한다. 대부분의 배포판에서는 `-lpthread` 를 걸지 않으면 `undefined reference to pthread_create` 로 링크 실패한다. `Threads::Threads` 타깃이 이 플래그를 자동으로 붙여준다. macOS 는 pthread 가 libSystem 에 있어 별도 링크가 필요 없으므로 `elseif (NOT APPLE)` 로 제외했다.

(d) **컴파일 정의 주입** — 위에서 본 캐시 변수가 여기서 매크로가 된다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    target_compile_definitions(tetris PRIVATE
        TETRIS_DEFAULT_RELAY_ENDPOINT="${TETRIS_DEFAULT_RELAY_ENDPOINT}"
        TETRIS_DEFAULT_META_URL="${TETRIS_DEFAULT_META_URL}")
    if (TETRIS_ENABLE_DEBUG_UI)
        target_compile_definitions(tetris PRIVATE TETRIS_ENABLE_DEBUG_UI=1)
    endif()
    if (TETRIS_ENABLE_NET_TRACE)
        target_compile_definitions(tetris PRIVATE TETRIS_ENABLE_NET_TRACE=1)
    endif()

    if (TETRIS_ENABLE_HTTPS AND OpenSSL_FOUND)
        target_compile_definitions(tetris PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
        target_link_libraries(tetris PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
```

문자열 캐시 변수를 큰따옴표로 감싸 매크로에 넣는 관용구(`NAME="${VALUE}"`)에 주의하라. 이렇게 해야 C++ 쪽에서 `std::string metaUrl = TETRIS_DEFAULT_META_URL;` 처럼 문자열 리터럴로 바로 쓸 수 있다.

`TETRIS_ENABLE_DEBUG_UI` 와 `TETRIS_ENABLE_NET_TRACE` 는 **정의 여부 자체**가 스위치다. 기본 빌드에서는 매크로가 아예 없으므로 관련 코드가 전처리 단계에서 사라진다 — 릴리스 바이너리에 디버그 오버레이 코드가 남지 않는다.

(e) **선택적 ONNX Runtime** — `TETRIS_BUILD_BOT=ON` 이 켜졌을 때만.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    # ------------------------------------------------------------------------
    # Optional: ONNX Runtime for Section C (Single vs Bot inference)
    # third_party/onnxruntime/ 에 공식 CPU 번들을 풀어두면 링크된다.
    # 없거나 OFF 면 bot/bot_onnx.cpp 가 스텁으로 빌드됨 → Load 항상 실패.
    # ------------------------------------------------------------------------
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

`TETRIS_HAS_ONNXRUNTIME=1` 매크로가 정의되면 `bot/bot_onnx.cpp` 가 실제 `Ort::Session` 경로로 컴파일된다 (정의 안 되면 스텁). CMake 는 **헤더 존재 여부만 사전 검사**하고, 그마저도 없으면 친절히 `fetch_onnxruntime.sh` 를 가리키는 에러로 실패한다. `linux-aarch64` 는 스크립트가 배치는 하지만 CMake 의 `else()` 분기가 `linux-x64` 경로를 하드코딩하고 있으므로, ARM64 리눅스에서 봇을 링크하려면 이 줄을 손봐야 한다.

(f) **rpath & .app 번들 메타** — 배포용 설정.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    # ------------------------------------------------------------------------
    # Section G — 플랫폼별 배포 설정 (rpath, macOS .app 번들 메타)
    # ------------------------------------------------------------------------
    if (APPLE)
        # macOS .app 번들 — scripts/release_macos.sh 가 이 구조를 전제.
        # Info.plist 를 configure_file 로 @변수@ 치환 후 빌드 디렉터리에 생성.
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
        # dylib 탐색: @executable_path/../Frameworks 를 rpath 에 추가.
        set_target_properties(tetris PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks")
    elseif (UNIX)
        # Linux — 번들에 동봉된 .so 를 찾기 위해 $ORIGIN/lib 을 rpath 로 설정.
        set_target_properties(tetris PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()

    # tetris_relay 도 Linux rpath 필요 (릴레이 단독 배포 시)
    if (UNIX AND NOT APPLE AND TETRIS_BUILD_RELAY)
        # tetris_relay 는 아래에서 별도 정의되지만, 이 시점에 아직 없을 수 있으므로
        # TETRIS_BUILD_RELAY 블록에서 직접 설정한다 (아래 참조).
    endif()
```

마지막 `if` 블록은 본문이 주석뿐인 **의도적 빈 블록**이다. CMake 는 위에서 아래로 실행되므로 이 시점에는 `tetris_relay` 타깃이 아직 존재하지 않는다. 블록을 지우지 않고 남긴 것은 "여기서 하고 싶었지만 순서 때문에 아래로 옮겼다" 는 기록이다.

**rpath 가 왜 중요한가.** macOS 와 Linux 의 동적 링커(`dyld`, `ld-linux`)는 실행 파일이 필요로 하는 `.dylib`/`.so` 를 시스템 경로(`/usr/lib`, `/usr/local/lib`)에서 찾는다. 하지만 배포 번들은 시스템에 아무것도 설치하지 않고 동봉된 라이브러리를 쓰고 싶다. rpath 는 실행 파일 안에 임베드되는 "탐색 경로 힌트" 다:

- macOS: `@executable_path/../Frameworks` — `Tetris.app/Contents/MacOS/tetris` 에서 `Tetris.app/Contents/Frameworks/libSDL2.dylib` 를 찾아간다.
- Linux: `$ORIGIN/lib` — 실행 파일과 같은 폴더의 `lib/libSDL2.so` 를 찾아간다.

Windows 는 rpath 개념이 없다 — DLL 은 "실행 파일과 같은 폴더" 를 자동으로 뒤지므로 배포 번들에서 DLL 을 `tetris.exe` 옆에 두기만 하면 된다.

### 3.5 `copy_assets` 커스텀 타깃

실행 파일은 빌드 디렉터리에 생성되지만 `Font/NanumGothic.ttf` 나 `Sounds/music.mp3` 는 소스 디렉터리에 있다. 게임은 상대 경로 `Font/...` 로 리소스를 여는데, 빌드 디렉터리에서 실행하면 파일을 못 찾는다. 해결은 빌드 시 자동으로 복사하는 커스텀 타깃이다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    # Copy assets (fonts + sounds + icons + model)
    #   assets/ 와 model/ 은 없을 수도 있으므로 directory 존재 검사 후 복사.
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
2. **`ALL`** — 기본 빌드에 포함(타깃 이름을 명시하지 않아도 실행). 반대로 `cmake --build build --target tetris` 처럼 **타깃을 명시하면 이 복사가 돌지 않는다.** 빌드 디렉터리에서 실행했을 때 폰트가 없다면 대부분 이 이유다.
3. **`DEPENDS tetris`** — 실행 파일이 먼저 빌드된 후 복사. 병렬 빌드 시에도 순서 보장.

`assets/`, `model/` 은 **없을 수도 있다**. `assets/` 에는 `images.cfg` 와 아이콘 PNG 가, `model/` 에는 배포용 ONNX 모델과 선택적 `bots.cfg` 가 들어간다. 권장 경로는 `model/bots/*.onnx` 이고, 예전 단일 모델 배포를 위해 legacy `model/*.onnx` 도 스캔한다. 둘 다 선택적이므로 `if (EXISTS ...)` 로 조건부 추가해 에러를 막는다.

### 3.6 타깃 2 — `tetris_py` (pybind11 모듈)

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Target: tetris_py (pybind11 module — Colab training + parity tests)
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_PY)
    # cmake 4.0+ removed FindPythonInterp/FindPythonLibs; tell pybind11 to use
    # the modern FindPython instead.
    set(PYBIND11_FINDPYTHON ON)
    # pybind11: prefer find_package (pip-installed), fall back to add_subdirectory
    # if a vendored pybind11 checkout is provided.
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

소스에는 `bindings/tetris_py.cpp`와 `TETRIS_SIM_SOURCES`가 들어간다. 렌더러·네트워크·플랫폼은 링크하지 않는다. Python은 `SimGame`만 보고, 학습 환경과 패리티 도구는 Python 레이어에서 따로 구성한다.

### 3.7 타깃 3 — `sim_hash_dump` / `worker_group_test` (회귀 테스트)

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Target: sim_hash_dump (determinism regression test — renderer-free)
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_TEST)
    add_executable(sim_hash_dump
        tests/sim_hash_dump.cpp
        ${TETRIS_SIM_SOURCES}
        ${TETRIS_SIM_HEADERS}
    )
    target_include_directories(sim_hash_dump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(worker_group_test
        tests/worker_group_test.cpp
        server/worker_group.h
    )
    target_include_directories(worker_group_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(worker_group_test PRIVATE Threads::Threads)
    endif()
endif()
```

`TETRIS_BUILD_TEST`는 서로 다른 계약을 검증하는 회귀 실행 파일들을 만든다. 현재 핵심 역할은 다음과 같다.

- `sim_hash_dump` — 오직 순수 시뮬만 링크. OS API 없음, 네트워크 없음. 고정 입력 스크립트를 여러 시드로 돌려 각 스텝의 `StateHash()`를 stdout에 찍으며, 순수 시뮬레이션 소스 집합이 처음 생길 때부터 계층 경계를 검증한다.
- `worker_group_test` — 릴레이 서버의 워커 스레드 수명 관리(`server/worker_group.h`)를 검증한다. 순수 시뮬레이션 검사와 같은 빌드 옵션 아래에 있지만 relay 동시성 계약을 소유하는 독립 타깃이다.

`sim_hash_dump`의 출력은 `python/tests/_sim_hash_dump.txt` 골든 파일과 비교한다. 플랫폼 간 `StateHash`가 한 비트라도 다르면 멀티플레이가 desync될 수 있다. 입력 스크립트·기본 시드·출력 행 수는 `tests/sim_hash_dump.cpp`가 소유하며, 문서는 고정된 개수를 복제하지 않는다. argv로 별도 시드를 넘겨 추가 진단할 수 있다.

### 3.8 타깃 4 — `tetris_relay` (릴레이 서버)

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Target: tetris_relay (matchmaking / relay server)
#
# 역할: TCP 접속 제한·인증·매칭·룸·결과 검증을 맡고, 게임 프레임은 전달한다.
#       GUI/오디오/시뮬 없음 — net/ 만 재사용. Linux/macOS/Windows 서버용.
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_RELAY)
    # relay 가 meta HTTP API 를 호출하려면 httplib 헤더와 http_client.cpp 필요.
    # third_party/httplib.h 는 TETRIS_BUILD_META 와 공유 — 릴레이만 빌드해도 필요.
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
        server/match_uuid.h
        server/player_conn.h
        server/player_session.h
        server/relay.h
        server/room.h
        server/worker_group.h
        net/socket.h
        net/framing.h
        meta/http_client.h
        meta/protocol.h
    )
    target_include_directories(tetris_relay PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    if (WIN32)
        target_link_libraries(tetris_relay PRIVATE ws2_32)
    else()
        # Linux/macOS: std::thread 는 pthread 를 필요로 함 (libstdc++)
        find_package(Threads REQUIRED)
        target_link_libraries(tetris_relay PRIVATE Threads::Threads)
    endif()
    if (TETRIS_ENABLE_HTTPS AND OpenSSL_FOUND)
        target_compile_definitions(tetris_relay PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
        target_link_libraries(tetris_relay PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    # Section G: Linux 배포 번들용 rpath
    if (UNIX AND NOT APPLE)
        set_target_properties(tetris_relay PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()
endif()
```

주목: 소스 목록에 `src/`가 없다. relay는 `SimGame`을 실행하지 않으며 `net/session.cpp`도 링크하지 않는다. 두 모드 모두 프레임 경계를 훑어 서버 전용 타입(`net::is_server_only_type`)은 걸러 내고, unranked 매치는 통과한 프레임의 내용을 보지 않고 원본 byte를 그대로 전달하며, ranked 매치에서만 `MATCH_SUMMARY`를 검증·가로챈다. meta API를 호출할 수 있어야 하므로 `third_party/httplib.h`는 relay 단독 빌드에도 필요하고, HTTPS를 쓰려면 OpenSSL 링크 블록도 함께 붙는다.

실행 인자는 `--port PORT` 와 meta 연동용 `--meta URL` / `--meta-secret SECRET` 이다(secret 은 환경변수 `TETRIS_RELAY_SECRET` 로도 줄 수 있고, `--meta` 를 줬는데 secret 이 없으면 기동을 거부한다). 기본 포트는 `7777` 이지만, 저장소의 relay/room smoke 테스트는 `7788` 을 하드코딩하고 있다 — [Part 7](./part7-relay-server.md) 의 테스트 절차를 따를 때 포트를 맞춰야 한다.

### 3.9 타깃 5 — `tetris_meta` (HTTP+SQLite 메타 서버)

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Target: tetris_meta (HTTP + SQLite metadata/leaderboard server)
#
# 역할: 별도 영속 호스트(저전력 Android/Termux 단말 등)에서 돌아가는 독립 서비스.
#       · SQLite 로 player/match/rating history/icon ownership 영속화
#       · cpp-httplib 로 guest/auth/icons/matches/leaderboard/health API 제공
#       · relay 에 영속 상태를 두지 않고 matchmaking 경로에서 HTTP 호출만 붙인다.
#
# 서드파티: third_party/sqlite3.{c,h} + third_party/httplib.h (헤더 온리).
#           두 파일 모두 벤더링(check-in)되어 있어야 한다 — repo 루트의
#           third_party/ 에 없으면 CMake 가 즉시 실패한다.
# -----------------------------------------------------------------------------
if (TETRIS_BUILD_META)
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3.c" OR
        NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_META=ON 이지만 third_party/sqlite3.{c,h} 가 없습니다. "
            "SQLite amalgamation 을 다운로드해 third_party/ 에 넣으세요 "
            "(https://www.sqlite.org/download.html).")
    endif()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib.h")
        message(FATAL_ERROR
            "TETRIS_BUILD_META=ON 이지만 third_party/httplib.h 가 없습니다. "
            "cpp-httplib single header 를 다운로드해 third_party/ 에 넣으세요 "
            "(https://github.com/yhirose/cpp-httplib).")
    endif()

    set(TETRIS_SQLITE3_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3.c")
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set_source_files_properties("${TETRIS_SQLITE3_SOURCE}" PROPERTIES COMPILE_OPTIONS "-w")
    elseif (MSVC)
        set_source_files_properties("${TETRIS_SQLITE3_SOURCE}" PROPERTIES COMPILE_OPTIONS "/w")
    endif()

    add_executable(tetris_meta
        meta/main.cpp
        meta/database.cpp
        meta/api_server.cpp
        ${TETRIS_SQLITE3_SOURCE}
        meta/database.h
        meta/api_server.h
        meta/elo.h
        meta/levels.h
        meta/protocol.h
    )
    target_include_directories(tetris_meta PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    # SQLite amalgamation — 기본 threadsafe(serialized) 모드로 컴파일.
    # WAL + mutex 는 C++ 래퍼에서 보강한다.
    target_compile_definitions(tetris_meta PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_RTREE=0
        SQLITE_DEFAULT_FOREIGN_KEYS=1
    )
    if (WIN32)
        # BCryptGenRandom is the fail-closed CSPRNG used for guest tokens.
        target_link_libraries(tetris_meta PRIVATE ws2_32 bcrypt)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(tetris_meta PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    if (UNIX AND NOT APPLE)
        set_target_properties(tetris_meta PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib")
    endif()
endif()
```

이 블록에서 놓치기 쉬운 네 가지:

1. **두 개의 존재 검사.** `sqlite3.c` 와 `sqlite3.h` 를 `OR` 로 함께 보고, `httplib.h` 를 따로 본다. 에러 메시지가 다운로드 URL 까지 알려준다.
2. **`-w` / `/w` 경고 억제.** SQLite amalgamation은 큰 외부 단일 C 파일이라 프로젝트의 경고 정책과 함께 컴파일하면 저장소가 통제하지 않는 진단이 쏟아질 수 있다. `set_source_files_properties`로 **이 파일 하나에만** 경고를 끈다. 프로젝트 전역 경고 설정을 낮추는 것과는 전혀 다르다.
3. **`meta/levels.h`.** XP → 레벨 곡선 테이블이며 헤더 목록에 포함돼 있다.
4. **플랫폼별 시스템 라이브러리.** Linux/macOS는 `Threads::Threads ${CMAKE_DL_LIBS}`로 SQLite의 스레드·동적 로딩 심볼을 제공한다. Windows는 HTTP socket용 `ws2_32`와 guest token CSPRNG인 `BCryptGenRandom`용 `bcrypt`를 링크한다.

`tetris_meta` 는 게임 클라이언트와 독립된 HTTP+SQLite 프로세스다. 실행 인자는 `--db PATH`, `--http HOST:PORT`, `--relay-secret SECRET`, `--allow-public-matches` 이며, 기본값은 `tetris.db` 와 `127.0.0.1:8080` 이다. 운영에서는 Caddy/Tunnel 뒤에 두고 `/v1/matches` 에 `X-Relay-Secret` 을 요구한다. secret 이 없으면 기본적으로 시작하지 않고, `--allow-public-matches` 는 로컬 테스트 전용이다.

### 3.10 라이브러리 링크 순서는 왜 중요한가

CMake 는 `target_link_libraries` 에 적은 순서대로 링커에 전달한다 (`-lA -lB -lC` 순으로). GCC/Clang 의 정적 링커는 **"왼쪽에서 오른쪽으로" 한 번만** 심볼 테이블을 훑는다. 만약 `A.o` 가 `libB` 의 심볼을 필요로 하면 반드시 `A` 가 `B` 보다 **먼저** 등장해야 한다. 그렇지 않으면 `B` 의 심볼이 그 시점에 필요하지 않은 것으로 판단돼 링커가 건너뛴다.

이 저장소에서는 이 문제가 겉으로 드러나지 않는데, 이유는:

1. `target_link_libraries` 가 받는 항목이 대부분 "누가 참조하는지 명확한 leaf" 들이다. 예: `gdi32` 는 Win32 표시 코드만 직접 사용한다.
2. MSVC 의 링커는 다수의 패스를 돌려 이 순서 민감도가 약하다.
3. `SDL2::SDL2` 나 `OpenSSL::SSL` 같은 IMPORTED 타깃은 내부에 `INTERFACE_LINK_LIBRARIES` 를 달고 있어, CMake 가 자동으로 전이적 의존성을 해결한다.

그래도 관례를 알아두면 좋다: **"사용하는 쪽 → 사용되는 쪽"** 순서다. 예컨대 `target_link_libraries(tetris PRIVATE opengl32 gdi32 gdiplus winmm ws2_32 xaudio2 ole32)` 에서 맨 뒤의 `ole32` 는 `xaudio2` 가 쓴다(COM). `tetris_meta` 의 `Threads::Threads ${CMAKE_DL_LIBS}` 도 sqlite3 오브젝트가 앞에 오고 그 심볼 제공자가 뒤에 오는 형태다.

---

## 4. `third_party/fetch_onnxruntime.sh` 의 역할

ONNX Runtime CPU 바이너리는 플랫폼별 번들이 크고, 사용자마다 필요한 플랫폼도 다르다(Linux 릴레이 호스트에서 봇을 돌릴 일은 없음). 그래서 Git 서브모듈로 묶지 않고 CMake 밖으로 빼서 **쉘 스크립트 하나**로 벤더링한다.

### 4.1 플랫폼 감지

**현재 소스 발췌 — `third_party/fetch_onnxruntime.sh`**

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
            # universal2 빌드가 arm64 + x86_64 모두 포함.
            echo "osx-universal2" ;;
        MINGW*|MSYS*|CYGWIN*|Windows_NT)
            echo "win-x64" ;;
        *)
            echo >&2 "[fetch_onnxruntime] Unknown OS: $os"; exit 1 ;;
    esac
}
```

`set -euo pipefail` 는 이런 스크립트의 기본기다. 오류 시 즉시 중단(`-e`), 미정의 변수 참조 시 중단(`-u`), 파이프 중간 실패도 감지(`-o pipefail`). 다운로드가 반쯤 실패한 채로 압축 해제 단계에 진입하는 사고를 막는다.

`DEST` 를 `$(cd "$(dirname "$0")" && pwd)` 로 계산하는 것도 관용구다. 스크립트를 어느 디렉터리에서 호출하든 항상 `third_party/onnxruntime` 을 가리킨다.

### 4.2 다운로드와 배치

**현재 소스 발췌 — `third_party/fetch_onnxruntime.sh`**

```bash
PLATFORM="$(detect_platform)"

# 파일 이름 결정 — 공식 릴리스 네이밍 규칙.
case "$PLATFORM" in
    win-x64)
        ARCHIVE="onnxruntime-win-x64-${ORT_VERSION}.zip"
        EXTRACT="unzip -q" ;;
    osx-universal2)
        ARCHIVE="onnxruntime-osx-universal2-${ORT_VERSION}.tgz"
        EXTRACT="tar xzf" ;;
    linux-x64)
        ARCHIVE="onnxruntime-linux-x64-${ORT_VERSION}.tgz"
        EXTRACT="tar xzf" ;;
    linux-aarch64)
        ARCHIVE="onnxruntime-linux-aarch64-${ORT_VERSION}.tgz"
        EXTRACT="tar xzf" ;;
    *) echo >&2 "Unsupported platform: $PLATFORM"; exit 1 ;;
esac

URL="${BASE_URL}/${ARCHIVE}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "[fetch_onnxruntime] Downloading $URL ..."
if command -v curl &>/dev/null; then
    curl -fSL "$URL" -o "$TMPDIR/$ARCHIVE"
elif command -v wget &>/dev/null; then
    wget -q "$URL" -O "$TMPDIR/$ARCHIVE"
else
    echo >&2 "Neither curl nor wget found."; exit 1
fi

echo "[fetch_onnxruntime] Extracting ..."
cd "$TMPDIR"
$EXTRACT "$ARCHIVE"

# 추출된 폴더 이름 찾기 (아카이브에 따라 이름이 약간 다를 수 있음).
EXTRACTED="$(find . -maxdepth 1 -type d -name 'onnxruntime-*' | head -1)"
if [ -z "$EXTRACTED" ]; then
    echo >&2 "Extraction failed — no onnxruntime-* directory found."; exit 1
fi

# 대상 구조 생성.
mkdir -p "$DEST/include" "$DEST/lib/$PLATFORM"

echo "[fetch_onnxruntime] Installing to $DEST ..."
# include/ — 공통 헤더
cp -f "$EXTRACTED/include/"*.h "$DEST/include/" 2>/dev/null || true

# lib/ — 플랫폼 라이브러리
case "$PLATFORM" in
    win-x64)
        cp -f "$EXTRACTED/lib/"*.lib "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        cp -f "$EXTRACTED/lib/"*.dll "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        ;;
    osx-universal2)
        cp -f "$EXTRACTED/lib/"*.dylib "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        ;;
    linux-*)
        cp -f "$EXTRACTED/lib/"*.so* "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        ;;
esac

echo "[fetch_onnxruntime] Done — $DEST ready for CMake -DTETRIS_BUILD_BOT=ON"
echo "  include/: $(ls "$DEST/include/" | wc -l) header(s)"
echo "  lib/$PLATFORM/: $(ls "$DEST/lib/$PLATFORM/" | wc -l) file(s)"
```

`trap 'rm -rf "$TMPDIR"' EXIT` 로 임시 디렉터리를 반드시 청소하고, 압축을 푼 뒤 실제 폴더 이름을 `find` 로 찾는다(릴리스마다 접미사가 조금씩 다르다). 복사 명령에 붙은 `2>/dev/null || true` 는 "그 확장자 파일이 없어도 계속" 이라는 뜻이다 — 릴리스별 파일 구성 차이를 흡수한다.

### 4.3 설계 결정 세 가지

**왜 CMake `FetchContent` 나 `ExternalProject_Add` 가 아닌가.** ONNX Runtime 은 공식 배포가 **이미 바이너리** 다 — CMake 빌드 스크립트가 들어 있지 않다. `FetchContent` 로 끌어와도 빌드할 수 없고, 단지 압축을 풀어 경로를 맞추는 일이 전부다. 그 일은 쉘이 더 잘 한다. 또한 CMake 시점에 네트워크 요청을 하면 오프라인 빌드가 깨진다. 스크립트는 한 번 실행하고 결과물을 커밋하지 않은 채 로컬에 남겨두는 편이 관리가 쉽다.

**버전 핀 전략.** 기본값 `1.18.1` 이 스크립트에 하드코딩돼 있다. 첫 번째 인자로 다른 버전(`./fetch_onnxruntime.sh 1.19.0`)을 넘길 수 있지만, CMake 는 이 값을 모른다 — 그저 `include/onnxruntime_cxx_api.h` 와 `lib/<platform>/` 하위의 확장자만 본다. API 레벨의 호환성은 Microsoft 가 세마 버저닝으로 보장한다. 이 프로젝트가 쓰는 `Ort::Session` API 는 대략 1.16 ~ 1.19 범위를 겨냥해 작성했지만, 이는 모든 패치 버전에서 측정한 보장이 아니라 가이드 정도로 본다 — 그래서 보통은 버전을 자주 건드릴 일이 없다.

**파일 배치 규칙.** 스크립트는 아카이브를 풀어 `third_party/onnxruntime/include/` (헤더) 와 `third_party/onnxruntime/lib/<platform>/` (라이브러리) 로 정리한다. `<platform>` 은 `win-x64`, `osx-universal2`, `linux-x64`, `linux-aarch64` 중 하나. CMakeLists 는 앞의 `TETRIS_BUILD_BOT` 블록에서 이 경로를 직접 참조한다.

CMake 쪽 에러 메시지도 스크립트를 정확히 가리킨다:

```text
FATAL_ERROR: TETRIS_BUILD_BOT=ON 이지만 third_party/onnxruntime/include/onnxruntime_cxx_api.h
가 없습니다. third_party/fetch_onnxruntime.sh 로 벤더링하거나 TETRIS_BUILD_BOT=OFF 로 빌드하세요.
```

Windows PowerShell 에서 bash 가 없다면 WSL 이나 Git Bash 를 써야 한다 — 또는 해당 아카이브를 수동으로 받아 같은 경로에 풀어도 된다.

---

## 5. 타깃별 빌드 조합

툴체인 설치는 [Part 0](./part0-project-setup.md) 에 있고, 릴리스 번들 생성은 [Part 12](./part12-hardening-and-release.md) 가 다룬다. 여기서는 그 사이 — **무엇을 빌드하고 싶을 때 어떤 옵션을 주는가** — 만 정리한다.

기본값은 게임 클라이언트와 회귀 테스트다. 나머지는 전부 명시적으로 켜야 한다.

| 만들고 싶은 것 | 옵션 | 산출물 |
|---|---|---|
| 게임 + 회귀 테스트 (기본) | (없음) | `tetris`, `sim_hash_dump`, `worker_group_test` |
| 시뮬레이션만 (헤드리스) | `-DTETRIS_BUILD_GAME=OFF` | `sim_hash_dump`, `worker_group_test` |
| 릴레이 서버 | `-DTETRIS_BUILD_RELAY=ON` | `tetris_relay` |
| 메타 서버 | `-DTETRIS_BUILD_META=ON` | `tetris_meta` |
| Python 모듈 | `-DTETRIS_BUILD_PY=ON -Dpybind11_DIR=$(uv run python -m pybind11 --cmakedir)` | `tetris_py` |
| 봇 포함 게임 | `-DTETRIS_BUILD_BOT=ON` (먼저 `third_party/fetch_onnxruntime.sh`) | ONNX 추론이 링크된 `tetris` |
| 서버만 (클라이언트 없이) | `-DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_META=ON` | `tetris_relay`, `tetris_meta` |

옵션은 CMake 캐시에 남으므로 조합할 수 있다.

```bash
cmake -S . -B build -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_META=ON
cmake --build build -j8
```

**`--target` 을 지정하지 않는 편이 낫다.** `copy_assets` 가 ALL 타깃이라 타깃을 명시하면 `Font/` 와 `Sounds/` 가 복사되지 않는다. 자세한 것은 §6.1 에 있다.

`TETRIS_BUILD_GAME` 이 기본 ON 이라는 점이 중요하다. 이 시리즈를 [Part 3](./part3-rendering-and-ui.md) 까지 따라오는 동안에는 게임 클라이언트의 소스가 아직 다 없으므로 반드시 `-DTETRIS_BUILD_GAME=OFF` 를 줘야 한다.

## 6. 플랫폼별로 다른 것

빌드 명령 자체는 세 플랫폼이 같다. 다른 것만 모은다.

### 6.1 Windows — multi-config 와 에셋 위치

Visual Studio 가 만드는 빌드 시스템은 multi-config 라서 Debug/Release 를 빌드할 때 고른다.

```powershell
cmake -B build
cmake --build build --config Release
.\build\Release\tetris.exe
```

**여기에 함정이 하나 있다.** 실행 파일은 `build\Release\` 에 생기는데 `copy_assets` 는 `${CMAKE_CURRENT_BINARY_DIR}` — 즉 `build\` 로 에셋을 복사한다. 그래서 `build\Release\` 로 들어가서 실행하면 폰트와 사운드를 찾지 못한다. 저장소 루트에서 경로를 지정해 실행하는 것이 가장 안전하다.

Win32 백엔드가 기본이므로 SDL2 없이 빌드된다. 창은 `CreateWindowExA`, GL 컨텍스트는 WGL(`opengl32.lib`), 오디오는 XAudio2 — 전부 Windows SDK 에 있다.

### 6.2 Linux — 단일 구성

Makefile/Ninja 는 single-config 라 `--config` 가 무시되고 산출물이 `build/` 에 바로 나온다.

```bash
cmake -B build
cmake --build build -j$(nproc)
./build/tetris
```

SDL2 백엔드가 기본이다(`TETRIS_USE_SDL2` 가 비-Windows 에서 ON). `libgl1-mesa-dev` 가 없으면 `find_package(OpenGL REQUIRED)` 가 configure 단계에서 멈춘다.

### 6.3 macOS — rpath 와 `.app`

빌드는 Linux 와 같다. 다른 점은 CMake 가 배포용 rpath 를 `@executable_path/../Frameworks` 로 박아둔다는 것이다. 번들 안에 dylib 을 넣고 배포할 수 있게 하려는 준비다 — 실제 번들 생성은 [Part 12](./part12-hardening-and-release.md) 의 릴리스 스크립트가 한다.

그래픽 쪽은 `OpenGL::GL` 하나만 링크되고, 그것은 `find_package(OpenGL)` 이 찾아낸 시스템 `OpenGL.framework` 다 — 별도 설치가 없다. macOS 의 OpenGL 은 10.14 부터 deprecated 표시가 붙어 있지만 3.3 Core 는 여전히 동작하고, **Core 프로파일이 아니면 3.x 자체를 주지 않는다.** SDL 백엔드가 `SDL_GL_CONTEXT_PROFILE_CORE` 를 명시적으로 요청하는 이유가 이것이다. 요청하지 않으면 2.1 호환 컨텍스트가 나와 `#version 330 core` 셰이더가 거부된다.

### 6.4 Termux (Android) — 릴레이 전용 크로스 빌드

릴레이 서버는 그래픽·오디오 의존성이 없어서 안드로이드 휴대폰에서도 돌아간다. 이 저장소의 계층 분리가 실제로 값을 하는 지점이다.

```bash
# Termux 앱에서
pkg install proot-distro
proot-distro install ubuntu
proot-distro login ubuntu

# Ubuntu proot 안에서
apt update && apt install -y cmake g++ git
git clone <repo> tetris && cd tetris

cmake -B build \
    -DTETRIS_BUILD_GAME=OFF \
    -DTETRIS_BUILD_RELAY=ON \
    -DTETRIS_BUILD_TEST=OFF
cmake --build build -j4

./build/tetris_relay --port 7777
```

`apt install cmake g++` 두 패키지면 끝난다. SDL2 도, 오디오 라이브러리도, OpenGL 개발 패키지도 필요 없다 — `TETRIS_BUILD_GAME=OFF` 면 `find_package(OpenGL)` 블록 자체가 실행되지 않는다. 같은 Wi-Fi 의 클라이언트가 휴대폰 내부 IP(보통 `192.168.x.x`)로 접속한다.

운영용으로 쓸 것이라면 [Part 12](./part12-hardening-and-release.md) 의 보안 기본값 — `TETRIS_RELAY_SECRET` 없이는 meta 연동이 시작되지 않는다는 것 — 을 먼저 읽는 편이 좋다.

## 7. 고치려면 어디를 건드리나

여기부터가 이 장의 실제 쓸모다. 하고 싶은 변경을 왼쪽에서 찾아 그 절로 가면 된다.

| 하고 싶은 것 | 난이도 | 절 |
|---|---|---|
| 공격량·점수·레벨 곡선 조정 | 쉬움 | §7.1 |
| 설정 항목 하나 추가 | 쉬움 | §7.2 |
| 새 UI 위젯 추가 | 쉬움 | §7.3 |
| 봇 교체 / 새 모델 추가 | 쉬움 | §7.4 |
| 셰이더 고치기 | 쉬움 | §7.5 |
| 새 도형 추가 (정점 속성 늘리기) | 보통 | §7.6 |
| 새 학습 알고리즘 추가 | 보통 | §7.7 |
| 새 네트워크 메시지 추가 | 보통 | §7.8 |
| 메타 API 엔드포인트 추가 | 보통 | §7.9 |
| 새 블록·규칙 변형 추가 | 어려움 | §7.10 |
| 새 입력 추가 (홀드 등) | 어려움 | §7.11 |
| 보드 크기 변경 | 어려움 | §7.12 |

난이도는 코드량이 아니라 **깨지는 계약의 수**로 매겼다. 아래로 갈수록 한 곳을 고치면 따라 고쳐야 할 곳이 늘어난다.

### 7.1 공격량·점수·레벨 곡선 조정

가장 안전한 종류의 변경이다. 전부 `src/sim_game.cpp` 한 파일 안에 있다.

| 바꾸고 싶은 것 | 위치 |
|---|---|
| 라인 클리어 → 공격 줄 수 | `attack_lines_for()` |
| 점수 계산 | `UpdateScore()` |
| 레벨업 주기와 낙하 속도 | `UpdateScore()` 내부의 `level` / `dropIntervalTicks` 계산 |
| 소프트 드롭 속도 | `softDropCounterTicks` 관련 상수 |

**함께 해야 할 일:** 골든 해시가 반드시 깨진다. 이 값들이 전부 상태 해시에 들어가기 때문이다. 의도한 변경이라면 기준 파일을 다시 뜬다.

```bash
cmake --build build --target sim_hash_dump
./build/sim_hash_dump > python/tests/_sim_hash_dump.txt
```

**주의:** 이 파일을 다시 뜨는 것은 "결정론이 깨졌다" 는 경보를 끄는 일이기도 하다. 규칙을 의도적으로 바꿨을 때만 해야 한다. 습관적으로 갱신하기 시작하면 이 테스트는 아무것도 지키지 못한다. 커밋 메시지에 왜 바꿨는지 남기는 편이 좋다.

**호환성:** 규칙이 바뀌면 **구버전 클라이언트와 멀티플레이가 불가능하다.** 같은 시드에서 다른 결과가 나오므로 접속 직후 DESYNC 로 떨어진다. 배포 중이라면 프로토콜 버전을 함께 올려 아예 붙지 않게 하는 편이 친절하다.

### 7.2 설정 항목 하나 추가

예를 들어 "고스트 피스 투명도" 같은 것을 추가한다고 하자.

1. `src/main.cpp` 의 `GameSettings` 구조체에 필드 추가.
2. 같은 파일의 `load_settings` / `save_settings` 에 키 한 줄씩 추가. 저장 형식이 `key=value` 텍스트라 파싱 코드가 단순하다.
3. `AppMode::Settings` 화면에 행을 하나 늘린다 — `enum RowKind` 에 항목을 추가하고 `kSettingsRows` 를 +1 한다.
4. 값이 실제로 쓰이는 곳에 연결한다.

자세한 흐름은 [Part 11](./part11-settings-and-options.md) 에 있다.

**주의할 계약:** 설정은 **결정론에 영향을 주면 안 된다.** 두 플레이어가 서로 다른 설정으로 같은 게임을 돌려도 해시가 같아야 한다. 그래서 흔들림·볼륨·해상도 같은 것은 안전하고, 낙하 속도나 보드 크기처럼 시뮬레이션에 닿는 값은 설정으로 빼면 안 된다. 판단 기준은 하나다 — **`SimGame::StateHash()` 가 그 값을 읽는가.**

기존 키가 없는 옛 `settings.cfg` 를 읽어도 기본값으로 동작해야 한다. `load_settings` 는 모르는 키를 무시하고, 없는 키는 손대지 않는 구조다.

### 7.3 새 UI 위젯 추가

`src/gui.h` / `src/gui.cpp`에 선언과 구현을 함께 추가한다. 현재 공개 함수 목록은 헤더가 유일한 기준이며, 모든 위젯은 같은 즉시 모드 입력 규칙을 따른다.

```text
gui_hover_rect · gui_button · gui_button_highlighted · gui_close_button
gui_checkbox · gui_slider · gui_value_selector · gui_modal_dim · gui_text_center
```

전부 **즉시모드**다. 위젯 객체도 상태도 없고, 매 프레임 "이 위치에 이걸 그리고 클릭됐으면 true 를 돌려줘" 를 호출한다. 그래서 새 위젯을 만드는 일은 `draw_rect` / `draw_text` / `gui_hover_rect` 를 조합하는 함수 하나를 쓰는 것이 전부다. 상태 관리는 호출부가 한다.

렌더러 프리미티브가 부족하면 `renderer/renderer.h` 에 그리기 함수를 추가한다 ([Part 3](./part3-rendering-and-ui.md)). 새 프리미티브를 `glb_rect` (축 정렬 사각형) 또는 `glb_quad` (네 꼭짓점을 직접 주는 사각형) 위에 올리면 view offset 더하기, 화면 밖 컬링, 텍스처 전환 시 자동 flush 가 전부 따라온다. 정점 형식을 바꿔야 하는 경우라면 §7.6 을 먼저 읽는다.

### 7.4 봇 교체 / 새 모델 추가

**모델만 바꾸는 경우** 코드를 건드릴 필요가 없다.

1. `.onnx` 파일을 `model/bots/` 에 넣는다.
2. `model/bots.cfg` 에 한 줄 추가해 표시 이름과 입력 간격을 준다.
3. 게임을 켜면 봇 선택 화면에 나타난다.

`src/main.cpp` 의 `discover_bot_roster()` 가 디렉터리를 훑어 목록을 만들기 때문이다. 자세한 형식은 [Part 9](./part9-rl-onnx-bot.md) 에 있다.

**모델의 구조가 다른 경우** 입출력 계약을 맞춰야 한다. `bot/bot_onnx.h` 에 적힌 이름과 shape 이 그대로여야 한다.

```text
입력  "board"   (1, 1, 20, 10)
      "current" (1, 7)
      "next"    (1, 7)
출력  "policy_logits" (1, 40)
      "value"         (1,)      ← 읽지 않지만 그래프에는 있어야 한다
```

이름이 하나라도 다르면 **로드는 성공하고 추론에서 실패한다.** 게임은 조용히 휴리스틱 봇으로 넘어가므로 증상이 "봇이 좀 약해졌네" 로만 보인다. 봇 선택 화면의 오류 메시지를 확인하는 습관이 필요하다.

### 7.5 셰이더 고치기

렌더러가 쓰는 셰이더는 **정점 하나, 조각 하나, 프로그램 하나**가 전부이고, 둘 다 `renderer/gl_shaders.h` 안의 raw string literal 이다. 별도 `.glsl` 파일도, 로딩 경로도, 런타임 리로드도 없다. 문자열을 고치고 다시 빌드하면 끝이다.

바꾸기 쉬운 것들:

| 하고 싶은 것 | 어디를 |
|---|---|
| 모서리 안티앨리어싱 폭 조절 | 조각 셰이더의 `smoothstep(-0.5, 0.5, d)` 범위 |
| 둥근 모서리 모양 변경 | `rounded_box_sdf()` 의 거리 함수 |
| 전역 톤 보정·색맹 팔레트 | 조각 셰이더에서 `fragColor` 를 내보내기 직전 |
| 좌표계 변경(예: y 위로 증가) | 정점 셰이더의 NDC 변환 두 줄 |

**셰이더 컴파일 오류는 빌드가 아니라 실행 시점에 난다.** GLSL 은 사용자 기계의 드라이버가 컴파일하므로, `renderer_init` 이 컴파일 로그를 stderr 에 그대로 찍고 초기화를 중단한다. 이때 `renderer_init` 은 `false` 를 반환하고 `main` 이 `platform_fatal_error` 로 네이티브 메시지박스를 띄운 뒤 종료한다 — 컴파일 로그 자체는 여전히 stderr 에만 남으므로, 셰이더를 고치는 동안은 콘솔에서 실행한다. 로그를 삼키지 않는 것이 중요한 이유는 드라이버마다 GLSL 프론트엔드가 달라서다 — 내 기계에서 통과한 코드가 남의 기계에서 막힐 수 있고, 그때 남는 단서가 이 로그뿐이다.

**주의할 계약:** 정점 셰이더의 `layout(location = N)` 번호와 `renderer.cpp` 의 `glVertexAttribPointer` 인덱스는 **같아야 한다.** 컴파일러도 링커도 이 대응을 검사하지 않는다 — §8 에 다시 나온다. 그리고 유니폼 이름(`u_screen`, `u_tex`)을 바꾸면 `renderer_init` 의 `gl_GetUniformLocation` 호출도 함께 고쳐야 한다. 이쪽은 실패해도 조용하다 — 위치가 `-1` 로 돌아오고 `glUniform*` 이 무시되어, 화면이 검거나 도형이 엉뚱한 곳에 그려진다.

셰이더를 나누고 싶은 유혹은 참는 편이 좋다. 도형마다 프로그램을 두면 그릴 때마다 `glUseProgram` 이 끼어들어 배칭이 끊기고, 프레임당 draw call 이 3~5 회에서 수백 회로 늘어난다. 지금 구조는 "차이를 셰이더가 아니라 **정점 속성으로** 넘긴다" 는 선택 위에 서 있다.

### 7.6 새 도형 추가 (정점 속성 늘리기)

`draw_rect` / `draw_rect_rounded` 처럼 기존 정점 형식으로 표현되는 도형이라면 `renderer.cpp` 에 함수를 하나 더하고 `glb_rect` 를 부르면 끝이다. 예컨대 테두리만 있는 사각형은 `glb_rect` 네 번이면 된다.

**정점 형식 자체를 늘려야 하는 경우**가 진짜 작업이다. 예를 들어 도형마다 그라디언트 방향을 주고 싶다면 속성이 하나 더 필요하다. 그때 고쳐야 할 곳이 **다섯 군데**이고, 하나라도 빠지면 컴파일은 되는데 화면이 조용히 깨진다.

1. **`renderer/gl_shaders.h` 의 정점 선언** — `layout(location = 7) in float a_gradient;` 를 추가하고, `out`/`in` varying 을 정점·조각 셰이더 양쪽에 짝지어 넣는다.
2. **`renderer/renderer.cpp` 의 `kFloatsPerVertex`** — 현재 14 다. 속성 하나를 늘리면 그 float 개수만큼 올린다. 이 상수 하나가 stride 계산과 `glDrawArrays` 의 정점 개수(`s_verts.size() / kFloatsPerVertex`) 를 동시에 결정하므로, 틀리면 정점이 어긋나 삼각형이 화면을 가로지르는 형태로 나타난다.
3. **`renderer_init` 의 `attribs[]` 테이블** — `{ location, size, offset }` 항목을 추가한다. `offset` 은 float 단위이므로 앞 속성들의 누적 개수여야 한다. 이 테이블이 `glVertexAttribPointer` + `glEnableVertexAttribArray` 호출로 그대로 펼쳐진다.
4. **`push_vertex()`** — 인자를 늘리고 `s_verts.insert` 의 초기화 리스트에 값을 **선언 순서대로** 넣는다. 순서가 어긋나면 색 자리에 좌표가 들어가는 식으로 조용히 망가진다.
5. **정점을 채우는 모든 호출부** — `glb_rect`, `glb_quad`, 그리고 그 둘을 부르는 `renderer.cpp` 의 `draw_rect` / `draw_rect_rounded`, `text_gl.cpp` 의 글리프 사각형, `image_gl.cpp` 의 이미지·회전 이미지. 새 속성의 기본값을 뭘로 둘지 여기서 정해야 한다.

**왜 컴파일러가 못 잡는가.** GPU 로 넘어가는 정점 버퍼는 그냥 `float` 배열이다. 타입 정보가 없고, "이 14 개 중 5~8 번째가 색" 이라는 해석은 전적으로 `glVertexAttribPointer` 가 준 offset/stride 에 달려 있다. C++ 쪽 구조체와 셰이더 쪽 선언을 이어주는 것은 사람이 맞춰 놓은 숫자 세 개(location, size, offset)뿐이고, 어긋나면 GL 은 에러를 내지 않고 그냥 다른 바이트를 읽는다.

증상으로 원인을 되짚는 요령:

| 증상 | 대개의 원인 |
|---|---|
| 도형이 화면 밖으로 늘어나거나 삼각형이 가로지른다 | `kFloatsPerVertex` 와 실제 push 개수 불일치 |
| 색이 좌표처럼 요동친다 | `push_vertex` 의 값 순서 또는 `attribs[]` 의 offset |
| 새 속성만 항상 0 | `glEnableVertexAttribArray` 누락 (테이블에 넣었는지 확인) |
| 새 속성이 무시되고 최적화된 듯 보인다 | 셰이더에서 그 varying 을 실제로 쓰지 않아 드라이버가 제거 |

마지막 줄이 특히 헷갈린다. GLSL 컴파일러는 결과에 영향을 주지 않는 입력을 **조용히 제거**하므로, 새 속성을 선언만 하고 `fragColor` 계산에 넣지 않으면 location 이 사라져 디버깅이 엉뚱한 방향으로 간다.

### 7.7 새 학습 알고리즘 추가

`python/train/`에 스크립트를 더할 때는 **학습 모델**과 **게임에 배포할 정책 모델**이 같은지 먼저 결정한다. 알고리즘이 달라도 `TetrisPolicyNet`을 직접 학습한다면 기존 스크립트의 저장과 평가 경로를 재사용할 수 있다.

직접 배포할 정책의 계약은 다음과 같다.

- `common.models.TetrisPolicyNet` 을 학습 대상으로 쓴다.
- `common.checkpoint.save_checkpoint()` 로 저장한다.
- 입력 shape, layer 구조, 출력 계약을 바꿨다면 `TetrisPolicyNet.ARCH_VERSION`을 올린다.

이 계약을 지키면 `python/netbot/export_onnx.py`가 체크포인트를 ONNX로 내보내고 C++ netbot이 실행한다. 현재 PPO·DQN·CBMPI 계열은 이 경로를 직접 쓴다.

MuZero-style 학습은 예외다. `MuZeroNet`의 representation·dynamics·prediction 구조를 담은 네이티브 체크포인트는 현재 C++ bot이 실행할 수 없다. `python/train/muzero_tetris.py`는 탐색 정책을 `TetrisPolicyNet`으로 증류해 `*.policy.pt`를 따로 저장하며, 이 증류된 파일이 기존 ONNX export의 입력이다. 새 모델 구조를 그대로 배포하고 싶다면 전용 exporter와 `bot/bot_onnx.cpp`의 입출력·추론 계약까지 함께 바꿔야 한다.

공통 부품은 `python/train/rl_common.py` 에 있다 — replay buffer, ε 스케줄, soft update, greedy 평가. 환경은 `common/env.py`(1인)와 `common/env_versus.py`(2보드 대전) 중 고른다.

**함께 해야 할 일:** `python/tests/test_training_scripts_static.py`의 대상 목록에 새 스크립트를 추가한다. 이 테스트는 파일을 자동 발견하지 않으며, 등록된 학습 진입점의 Python 문법을 PyTorch import 없이 검증한다. 알고리즘별 저장·재개·export 계약은 별도 테스트가 필요하다.

### 7.8 새 네트워크 메시지 추가

새 메시지는 C++ wire 정의, 소비자, Python mirror, relay 정책을 함께 검토한다. 하나라도 빠지면 빌드는 성공해도 양쪽 해석이 어긋날 수 있다.

1. **`net/framing.h`** — `enum class MsgType` 에 값 추가. **기존 값을 재사용하거나 중간에 끼워 넣지 마라.** 구버전과 붙었을 때 다른 메시지로 해석된다. 항상 뒤에 붙인다.
2. **`net/session.cpp`** — 송신 함수를 만들고 `handleFrame()` 의 `switch` 에 수신 분기를 추가한다.
3. **`python/netbot/framing.py`** — 같은 값을 Python 쪽 `MsgType` 에도 추가한다. 패리티 테스트가 이 둘을 대조한다.
4. **릴레이 통과 여부 결정** — `server/relay.cpp` 의 forwarder 는 기본적으로 바이트를 그대로 흘려보내므로 대개 아무것도 안 해도 된다. 다만 서버가 **들여다봐야 하는** 메시지라면(`MATCH_SUMMARY` 처럼) 파서에 분기를 추가한다.

**설계 지침:** C++ `parse_frames`는 type byte를 enum으로 보존하고 `Session::handleFrame`의 `default`가 모르는 타입을 무시한다. Python 파서는 `MsgType`에 없는 값을 그 프레임만 소비하고 버린다. relay의 일반 포워더는 모르는 게임 프레임도 raw bytes로 전달한다. 따라서 선택 기능은 구버전에서 무시될 수 있지만, `INPUT`처럼 진행에 필수인 메시지를 호환 협상 없이 추가하면 한쪽만 조용히 멈춘다. “모르는 타입을 무시한다”는 동작을 프로토콜 버전 협상 대신 사용하지 않는다.

**함께 해야 할 일:**

```bash
uv run python -m pytest python/tests/test_framing_parity.py -q
```

자세한 프레임 구조는 [Part 6](./part6-lockstep-networking.md) 에 있다.

### 7.9 메타 API 엔드포인트 추가

1. **`meta/api_server.cpp`** — 라우팅에 핸들러를 추가한다.
2. **`meta/protocol.h`** — 요청/응답 JSON 을 만들고 파싱하는 함수를 더한다. JSON 라이브러리를 쓰지 않고 직접 만든 최소 파서라, 필드를 추가하면 그 파서에도 손을 대야 한다.
3. **`meta/database.{h,cpp}`** — 스키마가 바뀐다면 `alter_if_needed()` 방식으로 **기존 DB 를 깨지 않고** 컬럼을 추가한다. 운영 중인 DB 를 마이그레이션할 수단이 이것뿐이다.
4. **`meta/http_client.{h,cpp}`** — 게임 클라이언트가 부를 API 라면 여기에도 추가.

**보안 경계:** 새 엔드포인트가 **게임 결과를 바꾸는 종류**라면 반드시 `X-Relay-Secret` 검사 뒤에 둔다. 클라이언트가 직접 부를 수 있으면 자기 전적을 스스로 조작할 수 있다. 반대로 조회만 하는 엔드포인트는 토큰 인증으로 충분하다. [Part 10](./part10-meta-and-ranking.md) 의 신뢰 경계 절을 참고.

**함께 해야 할 일:**

```bash
uv run python -m pytest python/tests/test_meta_db_smoke.py python/tests/test_relay_meta_smoke.py -q
```

### 7.10 새 블록·규칙 변형 추가

색이나 파티클만 바꾸는 스킨 작업과 새로운 모양을 게임에 넣는 규칙 변경을 분리한다. 스킨은 `src/colors.cpp`, 이미지 manifest, `Game`의 draw 경로만 바꾸면 되고 `StateHash()`는 그대로여야 한다. 새로운 piece ID나 특수 능력은 아래 계약을 함께 바꾼다.

1. **형상과 ID:** `src/sim_blocks.h`에 회전별 셀 좌표와 고유 ID를 정의하고 `src/sim_game.cpp`의 `SimGame::GetAllBlocks()` 반환 목록에 넣는다. 현재 생성기는 그 목록을 무작위 비복원 추출하는 bag이므로, ID·목록 순서·구성을 바꾸면 같은 seed에서도 piece 열이 달라진다. 새 모드는 기존 replay·lockstep과 호환되지 않는다.
2. **표현:** `src/colors.cpp`의 `GetCellColors()`가 새 ID를 안전하게 인덱싱할 수 있어야 하고, `Game::DrawNextAt` 계열에서 폭과 중심이 다른 모양의 preview 위치를 확인한다. 셀 수가 달라도 `SimBlock`의 vector는 담을 수 있지만 점수·충돌·T-spin 같은 규칙이 테트로미노를 전제하는지는 별도로 검토한다.
3. **결정론과 버전 경계:** 가방 구성, piece 상태, 특수 능력에 필요한 상태를 `StateHash()`와 `StateHashBreakdown()`에 포함한다. 기존 클라이언트와 섞이지 않도록 handshake에 명시적 ruleset/version 협상이 생기기 전에는 동일 relay 풀에서 서로 다른 규칙 빌드를 매칭하지 않는다.
4. **Python과 봇:** `python/common/__init__.py`의 `NUM_PIECE_TYPES`, `bot/placement.h`의 `kNumPieceTypes`, C++·Python 관측 one-hot shape를 함께 바꾼다. 입력 shape가 달라지므로 `TetrisPolicyNet.ARCH_VERSION`을 올리고 기존 체크포인트와 ONNX 모델을 거부·재학습한다. C++/Python placement 패리티도 다시 검증한다.
5. **콘텐츠 정책:** 표준 모드는 기존 tetromino와 공격표를 유지하고, 확장 블록은 별도 ruleset으로 두는 편이 안전하다. 그래야 랭킹, 밸런스 수치, replay가 서로 다른 규칙의 결과를 같은 것으로 취급하지 않는다.

따라서 “블록 추가”의 첫 구현은 shape class가 아니라 **ruleset 식별과 호환 정책**이다. 온라인 기능 없이 로컬 실험만 할 때도 골든 해시가 의도적으로 바뀌었는지 검토하고, 새 모드용 기준 파일을 기존 기준과 구분한다.

### 7.11 새 입력 추가 (홀드, 180도 회전 등)

입력 하나는 공용 비트 정의, 시뮬레이션, 플랫폼 매핑, 앱 입력 수집, 봇 행동 공간에 걸쳐 있다.

1. **`core/input.h`** — `enum InputBits`에 비트를 추가한다. 모든 입력 플래그는 현재 wire 단위인 `uint8_t` 안에 들어가야 한다. **비트 폭을 넘기면 프레이밍 포맷과 리플레이 계약이 함께 바뀐다** — `INPUT` payload가 tick당 1바이트라는 전제를 C++·Python·테스트가 공유한다.
2. **`src/sim_game.cpp`** — `SubmitInput()` 에 처리 추가. 새 상태가 생긴다면 (홀드는 "홀드한 블록" 이라는 상태가 생긴다) **`StateHash()` 에도 반드시 넣는다.** 빠뜨리면 두 클라이언트가 다른 상태인데 해시가 같아져, DESYNC 감지가 실패한다.
3. **`platform/win32.cpp` / `platform/sdl.cpp`** — 키를 읽는다. 두 백엔드 모두.
4. **`src/main.cpp`** — 키를 입력 마스크로 모으는 곳에 추가.
5. **봇 쪽 (선택)** — 봇이 그 입력을 쓰게 하려면 `bot/placement.cpp` 의 `expand_placement` 와 `python/netbot/input_expander.py` 를 **양쪽 다** 고쳐야 한다. 홀드처럼 액션 공간 자체가 커지는 변경이라면 `python/common/__init__.py` 의 `NUM_PLACEMENTS` 도 바뀌고, 그러면 학습된 모델이 전부 무효가 된다.

**함께 해야 할 일:** 골든 해시 재생성(§7.1), 그리고

```bash
uv run python -m pytest python/tests/test_placement_parity.py python/tests/test_framing_parity.py -q
```

### 7.12 보드 크기 변경

가장 넓게 퍼지는 변경이다. `src/sim_grid.h` 의 두 줄로 시작하지만 거기서 끝나지 않는다.

**현재 소스 발췌 — `src/sim_grid.h`**

```cpp
    static constexpr int kRows = 20;
    static constexpr int kCols = 10;
```

따라 움직이는 것들:

| 영역 | 무엇이 바뀌나 |
|---|---|
| 결정론 | 해시가 그리드 메모리를 통째로 먹으므로 골든 파일 재생성 필수 |
| 렌더링 | `src/game.cpp` 의 셀 크기·오프셋 계산. 720×640 논리 해상도에 안 들어가면 레이아웃도 |
| 가비지 | `InsertGarbage()` 의 홀 컬럼 범위 |
| RL 관측 | `python/common/__init__.py` 의 `BOARD_ROWS`/`BOARD_COLS`, `bot/placement.h` 의 `kBoardRows`/`kBoardCols` — **두 곳이 따로 있고 서로 참조하지 않는다** |
| RL 액션 | `NUM_COLS` 가 바뀌면 `NUM_PLACEMENTS` 가 바뀌고 신경망 출력 차원이 바뀐다 |
| 학습된 모델 | 전부 무효. `ARCH_VERSION` 을 올려 옛 체크포인트 로드를 막아야 한다 |

**핵심 함정:** C++ 과 Python 이 보드 크기 상수를 **각자 갖고 있다.** 한쪽만 고치면 컴파일도 되고 학습도 돌지만, 관측 텐서의 모양이 달라져 봇이 엉뚱하게 행동한다. `python/tests/test_placement_parity.py` 가 이걸 잡는 유일한 방어선이다.

---

## 8. 컴파일러가 잡아주지 않는 계약

위 시나리오들에 반복해서 나온 것들을 한자리에 모은다. **전부 어겨도 빌드가 성공하고, 한참 뒤에 이상한 증상으로 나타난다.**

| 계약 | 어기면 | 지키는 테스트 |
|---|---|---|
| `StateHash()` 가 시뮬 상태 전부를 덮는다 | DESYNC 를 감지 못 함 | `sim_hash_dump` diff |
| C++ 과 Python 의 placement/관측 로직이 같다 | 학습한 봇이 게임에서 다르게 행동 | `test_placement_parity.py` |
| C++ 과 Python 의 wire 포맷이 같다 | 프레임 파싱 실패 | `test_framing_parity.py` |
| ONNX 입출력 이름·shape | 봇이 조용히 휴리스틱으로 폴백 | 없음 — 봇 선택 화면 육안 확인 |
| 체크포인트 `ARCH_VERSION` | 구조가 다른 가중치가 조용히 로드됨 | `test_checkpoint_roundtrip.py` |
| 설정이 결정론에 영향을 주지 않는다 | 설정이 다른 두 클라이언트가 DESYNC | `sim_hash_dump` diff |
| `MsgType` 값을 재사용하지 않는다 | 구버전과 붙었을 때 메시지 오해석 | 없음 — 리뷰로 지킨다 |
| 셰이더의 `layout(location = N)` 과 `glVertexAttribPointer` 인덱스가 같다 | 화면이 조용히 깨진다 (색 자리에 좌표 등) | 없음 — 실행해서 눈으로 확인 |
| `kFloatsPerVertex` 가 실제 push 하는 float 개수와 같다 | 정점이 어긋나 삼각형이 화면을 가로지른다 | 없음 — 실행해서 눈으로 확인 |

테스트 칸이 비어 있는 네 줄을 짚어 둘 만하다. 자동으로 지킬 수 없는 것이 남아 있고, 그건 알고 있는 편이 낫다.

**GL 쪽 두 줄이 특히 지독하다.** 정점 버퍼는 타입 없는 `float` 배열이라, C++이 밀어 넣은 값과 셰이더가 읽는 속성을 이어주는 것은 사람이 맞춰 놓은 location·size·offset·stride뿐이다. 어긋나도 GL은 에러를 내지 않고 다른 바이트를 읽으며, `glGetError()`도 조용하다. 링커는 사용자 기계의 드라이버가 런타임에 컴파일하는 셰이더를 보지 못한다. 정점 작성 코드, stride/offset 선언, attribute location, 셰이더 입력을 한 변경으로 맞추고 렌더 데모에서 실제 픽셀을 확인해야 한다.

**여기 없는 계약도 하나 짚어 둔다 — "렌더 출력이 어디서나 같다" 는 계약은 없다.** GPU 래스터화 규칙은 벤더·드라이버·창 크기에 따라 경계 픽셀이 달라질 수 있고, 이 프로젝트는 그것을 검사하지 않는다. lockstep 이 desync 검출에 쓰는 것은 `SimGame::StateHash()` 이고 그 해시는 grid·블록·RNG·점수만 먹는다. 두 결정성은 처음부터 별개였다 — **잃은 것은 픽셀 단위 재현성이고, 잃지 않은 것은 게임 진행의 재현성이다.** 멀티플레이가 기대는 것은 후자뿐이다.

전체 회귀는 [Part 12](./part12-hardening-and-release.md) 의 검증 절에 한 세트로 정리돼 있다.

## 이 장에서 완성된 것

- 완성된 저장소의 디렉터리 경계와 빌드 타깃 관계를 한자리에서 본다.
- `CMakeLists.txt` 의 각 옵션이 어떤 소스와 라이브러리를 끌어오는지 추적할 수 있다.
- 무언가를 고치고 싶을 때 어느 파일을 건드려야 하고 무엇이 함께 깨지는지 안다.

## 마치며

이 시리즈는 엔진이 대신 해주던 일들을 하나씩 열어 보는 과정이었다. 창을 만들고, GL 컨텍스트를 세워 삼각형을 밀어 넣고, 소리를 섞고, 두 대의 컴퓨터를 같은 상태로 붙들고, 그 위에서 신경망을 돌렸다. 각각은 엔진에서 체크박스 하나였던 것들이다.

그래서 얻은 것은 테트리스가 아니라 **경계에 대한 감각**이다. 어디까지가 게임 규칙이고 어디부터가 표현인지, 무엇이 결정적이어야 하고 무엇은 달라도 되는지, 어떤 계약이 컴파일러의 보호를 받고 어떤 것은 사람이 지켜야 하는지. §8의 계약 표가 그 요약이다.

다음에 엔진을 쓸 때, 그 체크박스 뒤에 무엇이 있는지 알고 쓰게 된다면 이 시리즈는 제 몫을 한 것이다.
