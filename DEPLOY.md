# 배포 & 멀티플레이 실행 가이드

이 문서는 현재 리포에 실제로 구현된 기능 기준입니다. ELO/SQLite/HTTP API 는 `tetris_meta` 로 분리된 별도 서비스로 구현되어 있으며 (Section 6 참조), `--meta` 가 미지정인 unranked 모드도 그대로 동작합니다.

---

## 0. 구성 요소 요약

| 바이너리          | 플랫폼              | 역할                                                                                         |
|-------------------|---------------------|----------------------------------------------------------------------------------------------|
| `tetris`          | Windows / macOS / Linux | 게임 클라이언트. 싱글 / 봇 / 매치메이킹 / 커스텀 룸 / 채팅 지원. `--meta` 시 guest 토큰 자동 발급 + ELO 표시 |
| `tetris_relay`    | 어디서든 (헤드리스) | TCP accept → `QUEUE_JOIN` 수신 → 2명 모이면 `MATCH_FOUND` 전송 + 바이트 스트림 포워딩. `--meta` 시 토큰 검증 + 매치 결과 POST |
| `tetris_meta`     | Mac mini 등 (헤드리스) | HTTP + SQLite. guest 발급, 토큰 검증, 매치 결과 + ELO, 리더보드 — 4 endpoint                |
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
option(TETRIS_BUILD_META  "Build tetris_meta (HTTP+SQLite metadata)"   OFF)
option(TETRIS_BUILD_TEST  "Build sim_hash_dump"                        ON)
option(TETRIS_BUILD_PY    "Build pybind11 module"                      OFF)
option(TETRIS_BUILD_BOT   "Link onnxruntime (Single vs Bot inference)" OFF)
option(TETRIS_USE_SDL2    "Use SDL2 backend (cross-platform)"          ON on non-Windows, OFF on Windows)
```

| 옵션                  | 의도                                       | 결과 바이너리   | 플랫폼              | 의존성 (third_party 벤더링)           |
|-----------------------|--------------------------------------------|----------------|---------------------|---------------------------------------|
| `TETRIS_BUILD_GAME`   | 게임 클라이언트 (메뉴/렌더/네트)           | `tetris`       | Win / macOS / Linux | `httplib.h` (필수, guest 토큰 발급)   |
| `TETRIS_BUILD_RELAY`  | 무상태 매치메이킹/포워딩                   | `tetris_relay` | 어디서든            | `httplib.h` (필수, meta API 호출용)   |
| `TETRIS_BUILD_META`   | HTTP + SQLite 영속, ELO/리더보드           | `tetris_meta`  | Mac mini 등 영속 호스트 | `sqlite3.{c,h}` + `httplib.h`         |
| `TETRIS_BUILD_TEST`   | 결정론 회귀                                | `sim_hash_dump`| 어디서든            | (없음)                                |
| `TETRIS_BUILD_PY`     | pybind11 모듈                              | `tetris_py.so` | 학습용 환경         | pip 의 `pybind11`                     |
| `TETRIS_BUILD_BOT`    | ONNX Runtime 링크 (Single vs Bot)          | `tetris` 강화  | Win / macOS / Linux | `third_party/onnxruntime/`            |

`TETRIS_BUILD_BOT=ON` 시 `third_party/onnxruntime/` 에 공식 CPU 빌드가 있어야 합니다. 없으면:
```bash
./third_party/fetch_onnxruntime.sh   # 현재 OS/arch 자동 감지 + 다운로드
```

`TETRIS_BUILD_GAME` / `TETRIS_BUILD_RELAY` 가 `ON` 인데 `third_party/httplib.h` 가 없으면 CMake 가 즉시 실패합니다. Section 6.1 의 다운로드 명령으로 받아두세요.

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
# ranked: meta 서버 가리키기 (Section 6 참조)
./build/tetris_relay --port 7777 --meta http://mac-mini.local:8080
```

`--meta` 미지정 시 unranked 모드로 실행됩니다 — 매치는 정상 진행되지만 토큰 검증/`/v1/matches` POST 가 생략되어 점수가 기록되지 않습니다.

`tetris_relay` 도 `third_party/httplib.h` 를 요구합니다 (meta API 호출용). Section 6.1 의 다운로드 명령으로 미리 받아두세요.

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
tetris --meta   http://mac-mini.local:8080  # ranked: guest 토큰 자동 발급, 게임오버에 ELO 표시
```

또는 환경변수 `TETRIS_META_URL` 로 동일 효과 (CLI 우선). 첫 실행 시 `tetris_meta` 의 `POST /v1/guest` 를 호출해 토큰 + 초기 ELO(1200) 를 받고 user-data 경로(`%APPDATA%\Tetris\token` / `~/Library/Application Support/Tetris/token` / `${XDG_DATA_HOME:-~/.local/share}/Tetris/token`)에 저장합니다. 이후 실행마다 이 토큰을 `POST /v1/auth/verify` 로 검증하고, meta DB 가 리셋되어 404 가 돌아오면 자동으로 새 guest 를 재발급합니다.

`--meta` 미지정 또는 meta 서버 다운 시 unranked 모드로 폴백 — 매치메이킹/플레이 정상, 게임오버에 "ranking: disabled" 표시.

`--queue` 흐름:

1. 릴레이에 TCP 연결 → `QUEUE_JOIN` (빈 페이로드) 송신
2. 릴레이가 페어링하면 `MATCH_FOUND(role, seed)` 수신 (최대 5분 대기)
3. `Session::Adopt(sock, role, seed)` 로 HELLO/SEED 핸드셰이크 건너뛰고 즉시 게임 루프 진입
4. 이후 `INPUT`/`HASH`/`GAME_OVER_CHOICE` 는 릴레이를 통해 투명하게 전달됨
5. ranked + 양쪽 게임오버 시 클라가 `MATCH_SUMMARY` 를 릴레이에 전달 → 릴레이가 양쪽 보고 교차검증 후 meta `/v1/matches` POST → `MATCH_RESULT` 를 양쪽 클라에 회신 → 게임오버 화면에 ELO 변화 표시

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

## 6. Mac mini — `tetris_meta` 메타데이터 서버 배포

`tetris_meta` 는 별도 호스트 (예: 항상 켜둔 Mac mini) 에서 SQLite 파일을 들고 4개 HTTP endpoint 를 제공하는 독립 서비스입니다. relay 는 무상태 유지하고, ELO/매치 영속은 여기로 격리합니다.

### 6.1 third_party 사전 준비

`tetris_meta` 는 SQLite amalgamation 과 cpp-httplib 를 임베드합니다. 다음 두 파일을 `third_party/` 에 넣어야 합니다 (체크인되어 있지 않다면 직접 받기):

```bash
# SQLite amalgamation (sqlite3.c + sqlite3.h + sqlite3ext.h)
cd third_party
curl -L -O https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
unzip sqlite-amalgamation-3460000.zip
mv sqlite-amalgamation-3460000/sqlite3.{c,h} sqlite-amalgamation-3460000/sqlite3ext.h .
rm -rf sqlite-amalgamation-3460000*

# cpp-httplib (single header)
curl -L -o httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

### 6.2 빌드 (meta 만)

```bash
cmake -B build \
    -DTETRIS_BUILD_META=ON \
    -DTETRIS_BUILD_GAME=OFF \
    -DTETRIS_BUILD_RELAY=OFF \
    -DTETRIS_BUILD_TEST=OFF
cmake --build build --target tetris_meta -j
```

### 6.3 실행

```bash
./build/tetris_meta --db /var/lib/tetris/tetris.db --http 0.0.0.0:8080
```

CLI:
- `--db PATH` (기본 `tetris.db`) — SQLite 파일. 첫 실행 시 스키마 자동 생성, WAL 모드.
- `--http HOST:PORT` (기본 `0.0.0.0:8080`).

Endpoint:
- `POST /v1/guest` — 익명 player 생성, `{player_id, token, elo:1200}` 반환
- `POST /v1/auth/verify` — `{token}` → player 정보 (404 = 미등록 토큰)
- `POST /v1/matches` — 매치 결과 저장 + ELO 갱신, 양쪽 delta 반환
- `GET  /v1/leaderboard?limit=N` — top N (기본 20, 최대 100)

### 6.4 systemd unit (Linux Mac mini 또는 Linux 호스트)

`/etc/systemd/system/tetris-meta.service`:

```ini
[Unit]
Description=Tetris Meta API (HTTP+SQLite)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=tetris
Group=tetris
WorkingDirectory=/var/lib/tetris
ExecStart=/opt/tetris/tetris_meta --db /var/lib/tetris/tetris.db --http 127.0.0.1:8080
Restart=on-failure
RestartSec=3
StandardOutput=append:/var/log/tetris/meta.log
StandardError=append:/var/log/tetris/meta.log

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now tetris-meta
journalctl -u tetris-meta -f
```

### 6.5 macOS launchd 또는 nohup

순정 Mac mini 에서 systemd 가 없을 땐 `launchd` plist 가 정석이지만, 일단 띄워보려면:

```bash
nohup ./build/tetris_meta --db ~/tetris/tetris.db --http 127.0.0.1:8080 \
    >> ~/tetris/meta.log 2>&1 &
```

### 6.6 백업

SQLite 라이브 백업 (서비스 중단 없이):

```bash
sqlite3 /var/lib/tetris/tetris.db ".backup '/var/backups/tetris/tetris-$(date +%F).db'"
```

cron 일일 백업 예시 (`crontab -e`):

```
17 4 * * * /usr/bin/sqlite3 /var/lib/tetris/tetris.db ".backup '/var/backups/tetris/tetris-$(date +\%F).db'" && find /var/backups/tetris -name 'tetris-*.db' -mtime +30 -delete
```

### 6.7 방화벽 / 노출

권장: `--http 127.0.0.1:8080` 로 묶어 외부에서 직접 닿지 못하게 하고, **relay 만** LAN 에서 접근하게 합니다. relay 는 모바일 Linux 등에서 `--meta http://mac-mini.local:8080` 로 호출 — 이 경우 LAN bind 가 필요하므로 `--http 0.0.0.0:8080` 으로 두되 라우터/방화벽에서 LAN 외부 차단을 별도로 적용합니다. 게임 클라이언트도 `--meta` 를 직접 가리킬 수 있는데, 같은 LAN 에 있으면 OK이고 외부에서 쓰려면 reverse proxy + TLS 가 필요 (범위 밖).

---

## 7. 부록: CMake 변경 사항

`CMakeLists.txt` 의 `project(tetris CXX C)` — 원래 C++ 전용 프로젝트였지만 `third_party/sqlite3.c` (amalgamation) 를 빌드하기 위해 C 언어를 함께 활성화했습니다. `tetris_meta` 타겟만 실제로 C 를 쓰지만 `enable_language` 는 프로젝트 루트에서만 선언 가능해 이 위치가 강제됩니다. 그 외 타겟에는 영향 없음.

---

## 8. 범위 밖

- 인증 토큰을 넘어선 사용자 가입/로그인 UI (현재는 익명 guest 만)
- NAT 트래버설 (UPnP/STUN)
- TLS — 평문 TCP / 평문 HTTP 만 (reverse proxy 별도)
- 3인 이상 매치
- CI 자동 빌드 — 각 기기 수동 빌드
- `.AppImage` 포맷 — Linux 번들은 tar.gz (AppImage 는 후속)
- 코드 서명 / macOS 공증
