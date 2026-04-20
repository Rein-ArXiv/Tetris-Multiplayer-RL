# 배포 & 멀티플레이 실행 가이드

이 문서는 현재 리포에 실제로 구현된 기능 기준입니다. `MATCHMAKING.md` 의 ELO/DB/HTTP API 섹션은 장기 로드맵이며, 여기서는 범위 밖입니다.

---

## 0. 구성 요소 요약

| 바이너리          | 플랫폼              | 역할                                                                                         |
|-------------------|---------------------|----------------------------------------------------------------------------------------------|
| `tetris`          | Windows / macOS / Linux | 게임 클라이언트. 싱글 / 봇 / 매치메이킹 / 커스텀 룸 / 채팅 지원                         |
| `tetris_relay`    | 어디서든 (헤드리스) | TCP accept → `QUEUE_JOIN` 수신 → 2명 모이면 `MATCH_FOUND` 전송 + 바이트 스트림 포워딩     |
| `sim_hash_dump`   | 어디서든 (헤드리스) | 결정론 회귀 테스트. 크로스플랫폼 빌드 확인에도 유용                                       |

프로토콜 확장(`net/framing.h`):

```
QUEUE_JOIN    = 10   C→S, empty payload
QUEUE_CANCEL  = 11   C→S, empty payload
MATCH_FOUND   = 12   S→C, [role:1][seed:8 LE]   role: 1=HOST, 2=GUEST
ROOM_CREATE   = 13   C→S, empty payload
ROOM_JOIN     = 14   C→S, [code_len:1][code:N]
ROOM_INFO     = 15   S→C, [code_len:1][code:N][status:1][peer_count:1]
ROOM_LEAVE    = 16   C→S, empty payload
READY         = 17   C→S/S→C, [ready:1]
CHAT          = 20   C↔C, [text_len:2 LE][utf8:N]
```

`QUEUE_JOIN`/`ROOM_*`/`READY` 로 `MATCH_FOUND` 를 받은 뒤에는 `Session::Adopt()` 경로로 들어갑니다. 릴레이는 그 뒤의 프레임을 그대로 포워딩하고, 이 경로에서는 일반 P2P 의 `HELLO`/`SEED` 핸드셰이크를 다시 하지 않습니다.

`CHAT` 프레임은 UTF-8 바이트를 그대로 실을 수 있습니다. 다만 현재 게임 클라이언트의 채팅 입력 UI는 ASCII printable 문자만 받으므로, 한글 채팅을 넣으려면 입력/텍스트 렌더링 경로를 별도로 확장해야 합니다.

---

## 1. CMake 옵션 & 백엔드 매트릭스

```cmake
option(TETRIS_BUILD_GAME  "Build tetris (game client)"                 ON)
option(TETRIS_BUILD_RELAY "Build tetris_relay (matchmaking server)"    OFF)
option(TETRIS_BUILD_TEST  "Build sim_hash_dump"                        ON)
option(TETRIS_BUILD_PY    "Build pybind11 module"                      OFF)
option(TETRIS_BUILD_BOT   "Link onnxruntime (Single vs Bot inference)" OFF)
option(TETRIS_USE_SDL2    "Use SDL2 backend (cross-platform)"          ON on non-Windows, OFF on Windows)
```

`TETRIS_BUILD_BOT=ON` 시 `third_party/onnxruntime/` 에 공식 CPU 빌드가 있어야 합니다. 없으면:
```bash
./third_party/fetch_onnxruntime.sh   # 현재 OS/arch 자동 감지 + 다운로드
```

`tetris` 의 백엔드:

| `TETRIS_USE_SDL2` | platform      | audio            | text                              |
|-------------------|---------------|------------------|-----------------------------------|
| `OFF` (Win 기본)  | `win32.cpp`   | `audio.cpp` (XAudio2) | `text_win32.cpp` (GDI + wglUseFontBitmaps) |
| `ON`              | `sdl.cpp`     | `sdl_audio.cpp`  | `text_stb.cpp` (내장 5x7 비트맵)  |

`tetris_relay` 는 GUI/오디오 의존성이 전혀 없습니다 — `net/socket.cpp`, `net/framing.cpp`, `server/*` 만 컴파일하므로 Termux(Ubuntu proot, ARM64) 같은 환경에서도 빌드 가능합니다.

---

## 2. 기기별 빌드 & 실행

### 2.1 Termux (Android, Ubuntu proot, ARM64) — 릴레이 전용

```bash
pkg install proot-distro
proot-distro install ubuntu
proot-distro login ubuntu

apt update && apt install -y cmake g++ git
git clone <repo> tetris && cd tetris

cmake -B build \
    -DTETRIS_BUILD_GAME=OFF \
    -DTETRIS_BUILD_RELAY=ON \
    -DTETRIS_BUILD_TEST=OFF
cmake --build build -j4

./build/tetris_relay --port 7777
```

네트워크 메모:
- 휴대폰 내부 IP 확인 — `ip addr` 에서 WiFi 네트워크 대역 (보통 `192.168.x.x`).
- 같은 WiFi 안의 클라이언트는 그 IP 로 바로 접속 가능.
- 외부(인터넷) 접속은 공유기 포트 포워딩 + 공인 IP 가 필요합니다. 여기선 범위 밖.

### 2.2 macOS (Mac Mini, ARM64/Intel)

```bash
brew install cmake sdl2
git clone <repo> tetris && cd tetris

cmake -B build -DTETRIS_USE_SDL2=ON
cmake --build build -j$(sysctl -n hw.ncpu)

./build/tetris --queue 192.168.x.x:7777
```

macOS 에서는 `TETRIS_USE_SDL2=ON` 이 자동으로 선택되지만, 명시적으로 넘겨도 동일합니다. `-framework OpenGL` 을 CMake 가 자동 링크합니다.

#### macOS `.app` 번들 (Section G)

`scripts/release_macos.sh` 는 빌드된 `tetris` 바이너리를 `Tetris.app/Contents/`
구조로 번들링해 배포 가능한 `.app` 을 만듭니다. 핵심:

- `platform/macos/Info.plist.in` 을 CMake `configure_file` 로 `@PROJECT_VERSION@`
  치환 → `build/Info.plist` 생성.
- 실행 파일의 rpath 는 `@executable_path/../Frameworks` — 동봉된 SDL2 dylib 를
  그 경로에서 찾는다. 코드 서명 / 공증은 별도 단계 (배포 시 필요).

### 2.3 Linux (일반)

```bash
sudo apt install cmake g++ libsdl2-dev libgl1-mesa-dev
cmake -B build
cmake --build build -j$(nproc)

./build/tetris --queue 192.168.x.x:7777
```

헤드리스 서버에서만 쓸 거면 `-DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON` 으로 SDL2 설치 없이 빌드하세요.

`scripts/release_linux.sh` 는 바이너리와 동봉된 `.so` 를 `tetris-linux/` 디렉터리에
번들링합니다. 실행 파일 rpath 는 `$ORIGIN/lib` — 같은 폴더의 `lib/libSDL2.so*` 를
시스템 설치 없이 사용할 수 있게 합니다.

### 2.4 Windows (기존 경로 유지)

```powershell
cmake -B build -DTETRIS_BUILD_RELAY=ON
cmake --build build --config Release

.\build\Release\tetris.exe --queue 192.168.x.x:7777
.\build\Release\tetris_relay.exe --port 7777
```

Windows 에서 SDL2 경로로 빌드해 보고 싶으면 `-DTETRIS_USE_SDL2=ON` + `-DSDL2_DIR=...` 를 추가합니다. 기본값(`OFF`)은 블로그 시리즈와 일관되는 Win32 + XAudio2 + GDI 경로입니다.

---

## 3. 클라이언트 네트워크 모드

```
tetris --host 7777                          # 로컬 호스트, 상대가 --connect 로 접속
tetris --connect 192.168.1.5:7777           # 호스트에 직접 접속
tetris --queue  relay.example:7777          # 릴레이에 큐잉, 랜덤 매칭
tetris --relay relay.example:7777           # 메뉴 Matchmaking/Custom Room 에서 쓸 릴레이 주소 지정
```

`--queue` 흐름:

1. 릴레이에 TCP 연결 → `QUEUE_JOIN` (빈 페이로드) 송신
2. 릴레이가 페어링하면 `MATCH_FOUND(role, seed)` 수신 (최대 5분 대기)
3. `Session::Adopt(sock, role, seed)` 로 HELLO/SEED 핸드셰이크 건너뛰고 즉시 게임 루프 진입
4. 이후 `INPUT`/`HASH`/`GAME_OVER_CHOICE` 는 릴레이를 통해 투명하게 전달됨

---

## 4. 검증 체크리스트

```bash
# 1) 릴레이 단독 기동
./build/tetris_relay --port 7788 &

# 2) Python smoke test (두 클라이언트 페어링 + seed 일치 확인)
uv run --directory python python -m pytest tests/test_relay_smoke.py -v

# 2-1) Custom Room smoke test (ROOM_CREATE/JOIN/READY/MATCH_FOUND)
uv run --directory python python -m pytest tests/test_room_smoke.py -v

# 3) 결정론 회귀 (bash/macOS/Linux)
./build/sim_hash_dump
diff <(./build/sim_hash_dump) python/tests/_sim_hash_dump.txt

# 3-1) 결정론 회귀 (PowerShell)
.\build\Release\sim_hash_dump.exe | Set-Content .\build\sim_hash_dump.out
Compare-Object (Get-Content .\build\sim_hash_dump.out) (Get-Content .\python\tests\_sim_hash_dump.txt)

# 4) 프레이밍 패리티 (QUEUE_JOIN/MATCH_FOUND enum 포함)
uv run --directory python python -m pytest tests/test_framing_parity.py -v

# 5) 실제 멀티플레이 — 별도 기기 2대에서 (--queue 같은 릴레이 주소)
./build/tetris --queue <relay-ip>:7788
```

`test_relay_smoke.py` 와 `test_room_smoke.py` 는 `127.0.0.1:7788` 에 릴레이가 떠 있지 않으면 건너뜁니다. 순수 단위 테스트만 돌릴 때는 `test_framing_parity.py`, `test_placement_parity.py`, `test_checkpoint_roundtrip.py` 처럼 외부 프로세스가 필요 없는 테스트를 먼저 실행하세요.

---

---

## 5. 배포 번들 (받아서 바로 실행)

각 플랫폼별로 "더블클릭 / 한 줄 실행 → 즉시 플레이" 번들을 만드는 스크립트가 `scripts/` 에 있습니다.

### 5.1 Windows x64

```powershell
.\scripts\release_win.ps1              # 기본 (BOT=OFF)
.\scripts\release_win.ps1 -Bot         # 봇 포함 (ORT 필요)
.\scripts\release_win.ps1 -Sdl2        # SDL2 백엔드
```

산출물: `dist\tetris-win-x64.zip` — 압축 해제 후 `tetris.exe` 더블클릭.

포함 파일:
- `tetris.exe`, `tetris_relay.exe`, `sim_hash_dump.exe`
- `Font/`, `Sounds/`, `assets/` (아이콘), `model/` (봇 모델)
- `onnxruntime.dll` (BOT 모드일 때)
- `SDL2.dll` (SDL2 모드일 때)

### 5.2 macOS (arm64 + x86_64 Universal)

```bash
./scripts/release_macos.sh             # 기본
BOT=1 ./scripts/release_macos.sh       # 봇 포함
```

산출물: `dist/tetris-macos.tar.gz` (내부에 `Tetris.app`)
- `open Tetris.app` 또는 `Tetris.app/Contents/MacOS/tetris --queue ...`
- SDL2, ONNX Runtime 은 `Contents/Frameworks/` 에 동봉 (install_name_tool 로 rpath 설정)
- 코드 서명/공증은 별도 — 개인 배포 시 자기 Apple Developer 계정으로.

### 5.3 Linux x64

```bash
./scripts/release_linux.sh             # 기본
BOT=1 ./scripts/release_linux.sh       # 봇 포함
```

산출물: `dist/tetris-linux-x64.tar.gz`
- 압축 해제 후 `./tetris` 실행
- `lib/` 에 `libSDL2.so`, `libonnxruntime.so` 동봉, rpath 는 `$ORIGIN/lib`
- patchelf 가 있으면 추가 안전 패치 적용

### 5.4 ONNX Runtime 벤더링

봇(`Single vs Bot`) 을 포함하려면 ONNX Runtime CPU 바이너리가 필요합니다:

```bash
./third_party/fetch_onnxruntime.sh          # 자동 감지
./third_party/fetch_onnxruntime.sh 1.18.1   # 버전 지정
```

다운로드 후 `third_party/onnxruntime/{include,lib/<platform>}` 에 배치됩니다.

### 5.5 학습된 모델 준비

```bash
# Python 학습 환경에서:
uv run --directory python python -m netbot.export_onnx \
    checkpoints/step_N.pt  ../model/policy.onnx
```

`model/policy.onnx` 가 없으면 메뉴의 "Single vs Bot" 이 비활성화됩니다 (회색).

---

## 6. 범위 밖

- ELO 계산 / SQLite / HTTP API / 랭킹 UI (`MATCHMAKING.md` 5.1/5.2/5.5/10)
- 인증 토큰, 사용자명 (`QUEUE_JOIN` 은 빈 페이로드)
- NAT 트래버설 (UPnP/STUN)
- TLS — 평문 TCP 만
- 3인 이상 매치
- CI 자동 빌드 — 각 기기 수동 빌드
- `.AppImage` 포맷 — Linux 번들은 tar.gz (AppImage 는 후속)
- 코드 서명 / macOS 공증
