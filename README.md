# Tetris Multiplayer with Lockstep Networking

결정론적 P2P Lockstep 네트워킹 기반 멀티플레이어 테트리스. C++17로 작성했고,
렌더링은 raylib 없이 **Win32 + OpenGL (핸드메이드)** 또는 **SDL2 + OpenGL**
백엔드를 직접 구현했다. ONNX Runtime으로 RL 봇 추론, 파이썬 학습 파이프라인,
커스텀 매치메이킹 릴레이 서버까지 모두 포함한다.

## 빠른 시작

### 공통 (CMake)
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j
./tetris
```

- Windows 기본값: Handmade Win32 + XAudio2 경로 (`TETRIS_USE_SDL2=OFF`).
- macOS / Linux 기본값: SDL2 + OpenGL 경로 (`TETRIS_USE_SDL2=ON`).
- `-DTETRIS_USE_SDL2=ON` 을 Windows 에서도 지정하면 SDL2 경로로 빌드.

### 주요 CMake 옵션
| 플래그 | 기본 | 설명 |
|---|---|---|
| `TETRIS_BUILD_GAME`  | ON  | 게임 실행 파일 |
| `TETRIS_BUILD_TEST`  | ON  | `sim_hash_dump` 결정론 회귀 테스트 |
| `TETRIS_BUILD_PY`    | OFF | pybind11 모듈 (`tetris_py`) |
| `TETRIS_BUILD_RELAY` | OFF | 헤드리스 릴레이/룸 서버 (`tetris_relay`) |
| `TETRIS_BUILD_BOT`   | OFF | ONNX Runtime 링크 (Single vs Bot 활성화) |

## 멀티플레이어 실행

```bash
# 다이렉트 호스팅
./tetris --host 7777

# 다이렉트 접속
./tetris --connect 192.168.1.100:7777

# 릴레이 기반 랜덤 매칭
./tetris --queue relay.example.com:7777

# 커스텀 룸 (5자리 코드)
# CLI로 릴레이 주소를 지정한 뒤, Create/Join/Ready는 게임 메뉴에서 진행
./tetris --relay relay.example.com:7777
```

클라이언트가 지원하는 네트워크 CLI는 네 가지입니다.

| 옵션 | 용도 |
|---|---|
| `--host <port>` | 직접 접속용 호스트로 대기 |
| `--connect <host[:port]>` | 직접 호스트에 접속 |
| `--queue <host[:port]>` | 릴레이 랜덤 큐에 즉시 참가 |
| `--relay <host[:port]>` | 메뉴의 Matchmaking/Custom Room에서 사용할 릴레이 주소 지정 |

## 핵심 기능

- 결정론적 P2P Lockstep 동기화 (고정 60Hz 틱)
- DAS/ARR 기반 좌우 홀드 반복 + 소프트 드롭 속도 제한
- 10초 주기 자동 HASH 검증 + DESYNC 배너
- T-spin 점수/공격 판정 + 공격 라인 / 가비지 큐 / 화면 흔들림 / 콜아웃
- PING/PONG 하트비트 + 링크 단절 grace 복귀
- 5자리 코드 기반 커스텀 룸 + 랜덤 큐 매칭 서버
- 인-게임 채팅 (릴레이 투명 통과)
- 리플레이 기록 (F5/F6) / 상태 해시 출력 (H)
- ONNX Runtime 로컬 봇 추론 (Single vs Bot)
- 파이썬 lockstep 봇 클라이언트 + RL 학습 파이프라인
- Win32 핸드메이드 / SDL2 / 파이썬 pybind11 전부 지원

## 핫키

- **화살표**: 이동/회전 (좌우는 홀드 반복 지원)
- **Space**: 하드 드롭
- **F5/F6**: 리플레이 기록/저장
- **H**: 상태 해시 출력
- **R** (게임 오버): 재시작
- **Q** (게임 오버/취소): 타이틀/취소

## 프로젝트 구조

```
src/       게임 로직 (Game = SimGame + Draw)
core/      결정론 시스템 (틱, RNG, 해시, 리플레이)
net/       네트워킹 (Socket → Framing → Session, PING/ROOM/CHAT 포함)
server/    릴레이 + 룸 매치메이커 (tetris_relay)
bot/       ONNX Runtime 로컬 봇 추론
platform/  창 + 입력 (win32.cpp / sdl.cpp)
renderer/  OpenGL 2D 렌더러 + 텍스트 + shake + image
audio/     XAudio2 / SDL_OpenAudioDevice 기반 자체 믹서
bindings/  pybind11 → tetris_py 모듈
python/    RL 학습 + 네트봇 클라이언트 (common, netbot, sim, tests)
docs/blog/ 각 계층을 처음부터 만드는 10부 시리즈 (part0-9)
```

## 더 읽기

- **`docs/blog/part0` ~ `part9`** — 셋업, 창/렌더러/로직/루프/네트/RL/오디오,
  릴레이 서버, ONNX 봇까지 raylib 없이 직접 만드는 과정 블로그 시리즈.
- **`GUIDE.md`** — 코드를 처음 읽을 때 어디서부터 볼지 안내.
- **`ARCHITECTURE.md`** — 모든 모듈의 상세 레퍼런스.
- **`MATCHMAKING.md`** — 현재 릴레이/룸 구현 요약과 ELO/DB/API 장기 로드맵.
- **`DEPLOY.md`** — 플랫폼별 릴리스 번들 제작 절차.

## 요구사항

- **공통**: C++17, CMake 3.15+
- **Windows (Handmade)**: Windows SDK (WinSock2, XAudio2, GDI+)
- **macOS / Linux / Windows (SDL2)**: SDL2 개발 헤더 + OpenGL
- **RL / 봇**: Python 3.10+, PyTorch, pybind11, ONNX Runtime (선택)
