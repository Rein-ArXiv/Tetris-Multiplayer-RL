# 배포 & 멀티플레이 실행 가이드

이 문서는 현재 리포에 실제로 구현된 기능 기준입니다. `MATCHMAKING.md` 의 ELO/DB/HTTP API 섹션은 장기 로드맵이며, 여기서는 범위 밖입니다.

---

## 0. 구성 요소 요약

| 바이너리          | 플랫폼              | 역할                                                                                         |
|-------------------|---------------------|----------------------------------------------------------------------------------------------|
| `tetris`          | Windows / macOS / Linux | 게임 클라이언트. `--host` / `--connect` / `--queue` 3가지 네트워크 모드 지원            |
| `tetris_relay`    | 어디서든 (헤드리스) | TCP accept → `QUEUE_JOIN` 수신 → 2명 모이면 `MATCH_FOUND` 전송 + 바이트 스트림 포워딩     |
| `sim_hash_dump`   | 어디서든 (헤드리스) | 결정론 회귀 테스트. 크로스플랫폼 빌드 확인에도 유용                                       |

프로토콜 확장(`net/framing.h`):

```
QUEUE_JOIN   = 10   C→S, empty payload
MATCH_FOUND  = 12   S→C, [role:1][seed:8 LE]   role: 1=HOST, 2=GUEST
```

`MATCH_FOUND` 이후 릴레이는 프레임 파싱을 하지 않고 바이트를 그대로 포워딩하므로 기존 `SEED`/`INPUT`/`HASH`/`GAME_OVER_CHOICE` 흐름은 변경 없이 동작합니다.

---

## 1. CMake 옵션 & 백엔드 매트릭스

```cmake
option(TETRIS_BUILD_GAME  "Build tetris (game client)"              ON)
option(TETRIS_BUILD_RELAY "Build tetris_relay (matchmaking server)" OFF)
option(TETRIS_BUILD_TEST  "Build sim_hash_dump"                     ON)
option(TETRIS_BUILD_PY    "Build pybind11 module"                   OFF)
option(TETRIS_USE_SDL2    "Use SDL2 backend (cross-platform)"       ON on non-Windows, OFF on Windows)
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

### 2.3 Linux (일반)

```bash
sudo apt install cmake g++ libsdl2-dev libgl1-mesa-dev
cmake -B build
cmake --build build -j$(nproc)

./build/tetris --queue 192.168.x.x:7777
```

헤드리스 서버에서만 쓸 거면 `-DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON` 으로 SDL2 설치 없이 빌드하세요.

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

# 3) 결정론 회귀
./build/sim_hash_dump
diff <(./build/sim_hash_dump) python/tests/_sim_hash_dump.txt

# 4) 프레이밍 패리티 (QUEUE_JOIN/MATCH_FOUND enum 포함)
uv run --directory python python -m pytest tests/test_framing_parity.py -v

# 5) 실제 멀티플레이 — 별도 기기 2대에서 (--queue 같은 릴레이 주소)
./build/tetris --queue <relay-ip>:7788
```

---

## 5. 범위 밖

- ELO 계산 / SQLite / HTTP API / 랭킹 UI (`MATCHMAKING.md` 5.1/5.2/5.5/10)
- 인증 토큰, 사용자명 (`QUEUE_JOIN` 은 빈 페이로드)
- NAT 트래버설 (UPnP/STUN)
- TLS — 평문 TCP 만
- 3인 이상 매치
- CI 자동 빌드 — 각 기기 수동 빌드
