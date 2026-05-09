# Tetris Multiplayer RL

결정론적 lockstep 네트워킹을 기반으로 만든 멀티플레이어 테트리스입니다.
C++17, CMake, OpenGL 기반이며 raylib 없이 직접 구현한 Win32 백엔드와
SDL2 백엔드를 함께 사용합니다.

이 저장소에는 게임 클라이언트, 릴레이/룸 서버, HTTP+SQLite 메타 서버,
결정론 회귀 테스트, Python 시뮬레이션 바인딩, RL 학습용 환경/모델/export
코드, 선택형 ONNX Runtime 봇 추론 코드가 포함되어 있습니다.

## 현재 상태

- Windows 기본 빌드는 Handmade Win32 + OpenGL + XAudio2 경로입니다.
- macOS/Linux 기본 빌드는 SDL2 + OpenGL 경로입니다.
- `tetris`, `sim_hash_dump`, `tetris_relay`, `tetris_meta`는 CMake 타깃으로 분리되어 있습니다.
- `Single vs Bot`은 선택 기능입니다. `TETRIS_BUILD_BOT=ON`, ONNX Runtime, `model/policy.onnx`가 모두 있어야 활성화됩니다.
- Python 쪽은 Colab 부트스트랩, Gymnasium 환경, 정책 모델, 체크포인트, ONNX export까지 있습니다.
- 실제 RL 학습 알고리즘(PPO, MuZero, DQN 등)과 훈련 노트북은 아직 직접 작성해야 합니다.

## 빠른 시작

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\tetris.exe
```

릴리스 번들은 다음 스크립트로 만듭니다.

```powershell
.\scripts\release_win.ps1
```

산출물은 `dist\tetris-win-x64.zip`에 생성됩니다. 봇 포함 빌드는 ONNX Runtime과
`model/policy.onnx`가 준비된 뒤 실행합니다.

```powershell
.\scripts\release_win.ps1 -Bot
```

### Linux / macOS

Linux에서는 SDL2와 OpenGL 개발 패키지가 필요합니다.

```bash
# Ubuntu/Debian 예시
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/tetris
```

루트 `Makefile`은 CMake wrapper입니다.

```bash
make
make release-linux
```

## 주요 CMake 옵션

| 옵션 | 기본값 | 결과 | 설명 |
|---|---:|---|---|
| `TETRIS_BUILD_GAME` | ON | `tetris` | 게임 클라이언트 |
| `TETRIS_BUILD_TEST` | ON | `sim_hash_dump` | 결정론 해시 회귀 테스트 |
| `TETRIS_BUILD_RELAY` | OFF | `tetris_relay` | TCP 릴레이/룸/매치메이킹 서버 |
| `TETRIS_BUILD_META` | OFF | `tetris_meta` | HTTP+SQLite guest/ELO/리더보드 서버 |
| `TETRIS_BUILD_PY` | OFF | `tetris_py` | pybind11 기반 Python 시뮬레이션 모듈 |
| `TETRIS_BUILD_BOT` | OFF | `tetris` 내부 | ONNX Runtime 기반 로컬 봇 추론 |
| `TETRIS_USE_SDL2` | Windows OFF, 그 외 ON | 백엔드 선택 | SDL2 창/입력/오디오 백엔드 사용 |

서버까지 함께 빌드하려면 다음처럼 구성합니다.

```bash
cmake -S . -B build -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_META=ON
cmake --build build --config Release
```

Visual Studio 같은 multi-config generator에서는 `--config Release`를 사용하고,
Linux/macOS Makefile 또는 Ninja에서는 `-DCMAKE_BUILD_TYPE=Release`를 사용합니다.

## 멀티플레이 실행

직접 호스트/접속:

```bash
./tetris --host 7777
./tetris --connect 192.168.1.100:7777
```

릴레이 랜덤 매칭:

```bash
./tetris --queue relay.example.com:7777
```

커스텀 룸:

```bash
./tetris --relay relay.example.com:7777
```

클라이언트 CLI 옵션:

| 옵션 | 용도 |
|---|---|
| `--host <port>` | 직접 접속용 호스트로 대기 |
| `--connect <host[:port]>` | 직접 호스트에 접속 |
| `--queue <host[:port]>` | 릴레이 랜덤 큐에 즉시 참가 |
| `--relay <host[:port]>` | 메뉴의 Matchmaking/Custom Room에서 사용할 릴레이 주소 지정 |
| `--meta <http://host:port>` | ELO/리더보드용 `tetris_meta` URL |

`--meta`는 환경변수 `TETRIS_META_URL`로도 지정할 수 있습니다.

## 서버 구성

랭킹 멀티플레이를 쓰려면 `tetris_meta`와 `tetris_relay`를 함께 실행합니다.
`tetris_meta`는 guest 토큰, ELO, 리더보드를 담당하고, `tetris_relay`는 TCP
매치메이킹과 프레임 포워딩을 담당합니다.

메타 서버:

```bash
cmake -S . -B build-meta -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_META=ON
cmake --build build-meta --config Release
./build-meta/tetris_meta --db tetris.db --http 0.0.0.0:8080
```

릴레이 서버:

```bash
cmake -S . -B build-relay -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON
cmake --build build-relay --config Release
./build-relay/tetris_relay --port 7777 --meta http://127.0.0.1:8080
```

클라이언트:

```bash
./tetris --meta http://127.0.0.1:8080 --queue 127.0.0.1:7777
```

`--meta`가 없거나 메타 서버가 응답하지 않으면 unranked 모드로 동작합니다.

## RL / Bot

두 종류의 봇 경로가 있습니다.

- Python netbot: `.pt` 체크포인트를 읽어 네트워크 클라이언트처럼 접속합니다.
- In-game bot: `model/policy.onnx`를 C++ 게임이 직접 읽고 `Single vs Bot`에서 사용합니다.

현재 구현된 것은 학습 기반입니다.

- `bindings/tetris_py.cpp`: C++ `SimGame`을 Python으로 노출
- `python/common/env.py`: Gymnasium placement 환경
- `python/common/models.py`: `TetrisPolicyNet`
- `python/common/checkpoint.py`: 체크포인트 저장/로드
- `python/netbot/export_onnx.py`: `.pt` 체크포인트를 ONNX로 export
- `bot/bot_onnx.cpp`: C++ ONNX Runtime 추론

아직 포함되지 않은 것:

- PPO/MuZero/DQN 같은 실제 학습 루프
- Colab에서 바로 긴 학습을 수행하는 완성 training notebook
- 대전/가비지까지 반영한 경쟁형 RL 환경

Colab 기본 흐름:

```bash
# Colab에서 setup_colab.ipynb 실행 후
cd python
python -m netbot.export_onnx checkpoints/run.pt ../model/policy.onnx
```

Windows에서 in-game bot을 빌드하려면 ONNX Runtime을 `third_party/onnxruntime`에
준비한 뒤 실행합니다.

```powershell
.\scripts\release_win.ps1 -Bot
```

## 결정론 테스트

`sim_hash_dump`는 Windows/Linux/macOS에서 같은 입력 시퀀스가 같은 상태 해시를
내는지 확인하기 위한 기준 프로그램입니다.

```bash
cmake -S . -B build -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_TEST=ON
cmake --build build --config Release --target sim_hash_dump
./build/sim_hash_dump
```

Windows Visual Studio 빌드에서는 실행 파일 위치가 다릅니다.

```powershell
.\build\Release\sim_hash_dump.exe
```

Python 테스트는 Python 의존성이 설치되어 있어야 합니다.

```bash
pip install -r python/requirements.txt
python -m pytest python/tests
```

## 주요 기능

- 고정 60Hz 기반 결정론적 lockstep 시뮬레이션
- 7-bag RNG, 상태 해시, 리플레이 기록
- DAS/ARR 기반 좌우 반복 입력
- T-spin, 콤보, back-to-back, garbage queue
- P2P direct host/connect
- 릴레이 기반 랜덤 큐와 5자리 커스텀 룸
- 인게임 채팅, PING/PONG heartbeat, desync 배너
- HTTP+SQLite guest 토큰, ELO, 리더보드
- Python 시뮬레이션 바인딩과 RL 실험용 환경
- 선택형 ONNX Runtime 로컬 봇 추론

## 핫키

| 키 | 동작 |
|---|---|
| Arrow Left / Right | 좌우 이동 |
| Arrow Up | 회전 |
| Arrow Down | 소프트 드롭 |
| Space | 하드 드롭 |
| F5 / F6 | 리플레이 기록 / 저장 |
| H | 상태 해시 출력 |
| R | 게임 오버 후 재시작 |
| Q | 게임 오버 후 타이틀 복귀 또는 취소 |

## 프로젝트 구조

```text
src/        게임 조립 코드와 SimGame 기반 Game 래퍼
core/       입력 상수, RNG, 해시, 리플레이
platform/   Win32 / SDL2 창, 입력, 이벤트
renderer/   OpenGL 렌더러, 텍스트, 이미지, 화면 흔들림
audio/      XAudio2 / SDL audio mixer
net/        TCP socket, framing, lockstep session
server/     relay, room, matchmaking server
meta/       HTTP+SQLite meta server와 meta client
bot/        placement helper와 ONNX Runtime bot
bindings/   pybind11 tetris_py 모듈
python/     RL common layer, netbot, tests, Colab setup
scripts/    Windows/Linux/macOS release scripts
docs/blog/  구현 과정을 정리한 블로그 시리즈
```

## 요구사항

- 공통: C++17, CMake 3.15+
- Windows: Visual Studio Build Tools 또는 동등한 MSVC 환경, Windows SDK
- Linux/macOS SDL2 빌드: SDL2 개발 헤더, OpenGL
- Python/RL: Python 3.10+, PyTorch, Gymnasium, pybind11, pytest
- Meta server: vendored `third_party/sqlite3.{c,h}`와 `third_party/httplib.h`
- In-game bot: ONNX Runtime CPU bundle, `model/policy.onnx`

## 주의 사항

- `dist/`는 릴리스 스크립트가 만드는 산출물이며 보통 Git에 커밋하지 않습니다.
- `TETRIS_BUILD_BOT=OFF`이면 게임은 정상 빌드되지만 `Single vs Bot`은 비활성화됩니다.
- `Sounds/drop.mp3`, `Sounds/garbage.mp3`는 코드에서 참조하지만 현재 기본 사운드 폴더에는 없을 수 있습니다. 없어도 빌드는 실패하지 않고 해당 효과음만 재생되지 않습니다.
- 코드를 처음 읽는다면 `GUIDE.md`, `ARCHITECTURE.md`, `docs/blog/part0`부터 순서대로 보는 것을 권장합니다.
