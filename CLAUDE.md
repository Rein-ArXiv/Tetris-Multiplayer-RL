# CLAUDE.md

이 파일은 Claude Code (claude.ai/code)가 이 저장소에서 작업할 때 참고할 가이드를 제공합니다.

## 프로젝트 개요

C++와 Raylib로 구현한 P2P Lockstep 네트워크 멀티플레이어 테트리스입니다. 핵심 아키텍처는 고정 틱 시뮬레이션(60Hz)과 피어 간 입력 동기화를 기반으로 한 결정론적 게임플레이를 중심으로 설계되었습니다.

## 빌드 명령어

### Windows (w64devkit + raylib)

```bash
# mingw32-make로 빌드
mingw32-make PLATFORM=PLATFORM_DESKTOP RAYLIB_PATH=C:/raylib/raylib

# 빌드 산출물 정리
mingw32-make clean
```

**사전 요구사항**:
- raylib이 `C:/raylib`에 설치되어 있어야 함
- w64devkit 셸 환경 (`C:/raylib/w64devkit/w64devkit.exe`)
- 런타임 DLL: `lib/libstdc++-6.dll`, `lib/libgcc_s_dw2-1.dll` (exe 위치에 복사하거나 PATH에 추가)

**실행 파일**: `tetris.exe` (리소스 경로 유지를 위해 프로젝트 루트에서 실행)

### Linux/Mac (CMake)

```bash
# 설정 및 빌드
mkdir -p build && cd build
cmake ..
cmake --build . -j

# 실행
./tetris
```

**사전 요구사항**: raylib이 pkg-config를 통해 또는 `/usr/local`에 설치되어 있어야 함

## 네트워크 아키텍처

네트워킹 시스템은 모듈성과 플랫폼 독립성을 위해 계층화되어 있습니다:

### 계층 구조

```
Application (main.cpp) → Session (net/session.*) → Framing (net/framing.*) → Socket (net/socket.*)
```

1. **Socket Layer** (`net/socket.*`): 플랫폼 독립적 TCP 추상화 (Windows WinSock / Linux BSD 소켓)
2. **Framing Layer** (`net/framing.*`): 길이 프리픽스 + 타입 + 페이로드 + 체크섬(FNV-1a)을 사용한 메시지 직렬화
3. **Session Layer** (`net/session.*`): P2P Lockstep 동기화, 스레드 I/O, 핸드셰이크 프로토콜

### 스레드 모델

- **메인 스레드**: 렌더링, 오디오, 입력 샘플링, 게임 시뮬레이션
- **I/O 스레드**: 논블로킹 소켓 수신/송신, 메시지 파싱, 프레임 디스패치
- **Accept 스레드** (호스트만): 클라이언트 연결될 때까지 `tcp_accept()`에서 블로킹

스레드 간 데이터 교환은 뮤텍스로 보호되는 큐와 atomic 플래그를 사용합니다.

### Lockstep 동기화

양쪽 피어는 틱 N을 시뮬레이션하기 전에 틱 N에 대한 모든 입력을 받아야 합니다. "안전 틱"은 다음과 같이 계산됩니다:
```
safe_tick = min(local_sent_tick, remote_max_tick) - input_delay
```

**주요 개념**:
- `inputDelay` (기본값 2틱): 네트워크 지연 버퍼
- `startDelay` (기본값 120틱): 연결 안정화를 위한 게임 전 카운트다운
- 입력은 매 틱마다 전송됨 (60 패킷/초) — 향후 20-30Hz 배치 전송으로 최적화 예정

### 네트워크 세션 흐름

#### 핸드셰이크 (호스트)
```cpp
session.Host(port, SeedParams{seed, startDelay, inputDelay, Role::Host});
// → 포트 바인딩 → 클라이언트 대기 → HELLO + SEED 전송
```

#### 핸드셰이크 (클라이언트)
```cpp
session.Connect(host_ip, port);
// → 호스트에 연결 → HELLO 전송 → SEED 파라미터 수신
```

#### 게임 루프
```cpp
// 메인 스레드: 매 프레임
uint8_t input = SampleInput();
session.SendInput(localTick, input);

uint8_t remoteInput;
if (session.GetRemoteInput(simTick, remoteInput)) {
    game.SubmitInput(input | remoteInput);  // 합쳐진 입력 적용
    game.Tick();  // 시뮬레이션 진행
    simTick++;
}
```

### 메시지 타입

`net/framing.h`에 정의됨:
- `HELLO` / `HELLO_ACK`: 프로토콜 버전 교환
- `SEED`: 호스트로부터 게임 파라미터 (seed, start_tick, input_delay)
- `INPUT`: 틱별 입력 비트마스크 (5비트: LEFT/RIGHT/DOWN/ROTATE/DROP)
- `HASH`: desync 감지를 위한 상태 해시
- `PING` / `PONG`: Keepalive

## 결정론 코어

`core/`에 위치 — 게임 로직 의존성은 순수하게 결정론적이어야 합니다:

### 고정 틱 시스템
- **속도**: 60Hz (`core/constants.h`의 `TICKS_PER_SECOND`)
- **분리**: 로직 업데이트(틱 기반)는 렌더링(프레임 기반)과 분리됨
- 모든 게임플레이 진행은 `Game::Tick()`에서 발생, 프레임별로 발생하지 않음

### RNG (`core/rng.h`)
- **알고리즘**: 빠르고 반복 가능한 시퀀스를 위한 XorShift64*
- **초기화**: SEED 메시지 핸드셰이크를 통해 합의된 세션 시드
- **사용법**: 블록 생성을 위한 `rng.nextUInt(max)`
- **상태**: RNG 상태는 게임 상태의 일부 (해시/스냅샷에 포함됨)

### 입력 시스템 (`core/input.h`)
- **형식**: 틱별 8비트 비트마스크 (5개 버튼: `INPUT_LEFT | INPUT_RIGHT | INPUT_DOWN | INPUT_ROTATE | INPUT_DROP`)
- **적용**: `Game::SubmitInput(uint8_t mask)`가 현재 상태에 입력 적용
- **네트워크**: 입력은 틱당 단일 바이트로 전송됨

### 상태 해싱 (`core/hash.h`)
- **목적**: 상태 지문을 비교하여 피어 간 desync 감지
- **계산**: `Game::ComputeStateHash()`는 그리드, 현재 블록, RNG 상태, 점수, 중력 카운터에 FNV-1a 사용
- **사용법**: 피어 간 주기적으로 해시 비교; 불일치는 결정론 버그를 나타냄

### 리플레이 시스템 (`core/replay.*`)
- **형식**: 텍스트 기반 (시드 + 틱별 입력 쌍)
- **기록**: F5로 시작, F6로 저장 (`out/replay.txt`)
- **재생**: 시드를 로드하고 입력을 재생하여 정확한 게임 세션 재현
- **디버깅**: 비결정론 문제 진단에 필수적

## 코드 구조

```
src/              UI, 렌더링, 오디오 (raylib 의존)
  ├─ main.cpp     애플리케이션 진입점, 메뉴, 네트워크 CLI 인자, 듀얼 보드 렌더링
  ├─ game.*       Game 클래스: 보드 로직, 틱 진행, 입력 처리
  ├─ grid.*       그리드 상태 (배치된 블록)
  ├─ block.*      테트로미노 모양과 회전
  └─ colors.*     비주얼 팔레트

core/             결정론적 시뮬레이션 (stdlib 외 외부 의존성 없음)
  ├─ constants.h  TICKS_PER_SECOND 정의
  ├─ input.h      입력 비트마스크 상수
  ├─ rng.*        XorShift64* RNG
  ├─ hash.h       FNV-1a 해싱
  └─ replay.*     리플레이 기록/재생

net/              네트워킹 계층
  ├─ socket.*     TCP 래퍼 (Windows/Linux)
  ├─ framing.*    메시지 직렬화
  └─ session.*    P2P Lockstep 프로토콜

Font/, Sounds/    에셋 (폰트, 오디오)
```

**중요**: `src/blocks.cpp`는 `#include`를 통해 `game.h`에 직접 포함되며, 별도로 컴파일되지 않습니다. Makefile/CMakeLists가 이를 제외합니다.

## 멀티플레이어 실행

### CLI 인자

```bash
# 포트 7777에서 호스트
./tetris --host 7777

# 192.168.1.100:7777의 호스트에 연결
./tetris --connect 192.168.1.100:7777
```

### 게임 내 HUD

화면 하단에 표시:
- **NET**: 연결 상태 (CONNECTED/DISCONNECTED)
- **TICKS**: `localSent=N remoteMax=M sim=K`
  - `localSent`: 피어에게 전송한 마지막 틱
  - `remoteMax`: 피어로부터 받은 마지막 틱
  - `sim`: 시뮬레이션된 마지막 틱

### 듀얼 보드 렌더링

네트워크로 연결되면, 로컬(좌측)과 원격(우측) 게임 보드가 나란히 렌더링되어 상태를 비교할 수 있습니다.

## 개발 가이드라인

### 게임 로직 수정 시

1. **결정론 유지**: 모든 랜덤성은 `rng`를 통해야 하며, 로직에 시스템 시간/부동소수점 모호성 없어야 함
2. **상태 해시 테스트**: 변경 후 네트워크 게임을 실행하고 `H` 키로 해시 일치 확인 (콘솔에 출력)
3. **리플레이 사용**: 세션 기록 (F5/F6)으로 버그를 결정론적으로 재현

### 네트워킹 수정 시

1. **계층 경계**: socket/framing/session 관심사를 분리된 상태로 유지
2. **스레드 안전성**: 모든 공유 상태 (세션 큐, 플래그)는 뮤텍스/atomic 사용 필수
3. **메시지 프로토콜**: 프레임 형식 변경 시 HELLO 교환에서 버전 업데이트 필요

### 최적화 로드맵 (계획됨)

- **패킷 배치**: 60Hz 개별 전송을 20-30Hz 배치로 감소 (3틱 윈도우)
- **입력 압축**: 반복 입력에 대한 런-렝스 인코딩
- **재연결**: 타임아웃 처리 및 상태 스냅샷 기반 복구
- **UDP 옵션**: TCP head-of-line 블로킹 대비 낮은 지연을 위한 커스텀 신뢰성 계층

## 일반적인 작업

### 새 입력 타입 추가

1. `core/input.h`에 비트 정의 (예: `INPUT_HOLD = 0x20`)
2. `main.cpp`의 `SampleInput()`을 업데이트하여 키 확인
3. `Game::SubmitInput()` 로직에서 처리
4. 멀티플레이어에서 해시가 여전히 일치하는지 확인

### 틱 속도 변경

1. `core/constants.h`에서 `TICKS_PER_SECOND` 수정
2. 세션 파라미터에서 `inputDelay`와 `startDelay`를 비례적으로 조정
3. 전체 재컴파일 (헤더 변경)

### Desync 디버깅

1. 해시 로깅 활성화: 네트워크 게임 중 `H` 키 누름
2. 같은 틱에서 양쪽 클라이언트의 콘솔 출력 비교
3. 불일치 시 리플레이를 사용하여 분기점 격리
4. 비결정론 확인: 시스템 호출, 초기화되지 않은 메모리, 부동소수점 정밀도

## 기술 문서

`docs/`의 상세 구현 가이드:
- `NETWORKING_OVERVIEW.md` — 전체 네트워크 아키텍처
- `SOCKET_LAYER.md` — 플랫폼 소켓 추상화
- `FRAMING_PROTOCOL.md` — 메시지 형식 명세
- `SESSION_LAYER.md` — Lockstep 프로토콜 세부사항

## 플랫폼 노트

### Windows
- WinSock2 사용 (`ws2_32` 라이브러리)
- `net::net_init()`을 통한 `WSAStartup()/WSACleanup()` 필요
- C++17 표준, MinGW g++로 컴파일

### Linux
- BSD 소켓 사용 (`sys/socket.h`, `netinet/in.h`)
- 스레딩 및 동적 링킹을 위한 `-lpthread -ldl -lrt` 필요
- 윈도우 시스템을 위한 X11 또는 Wayland (raylib이 처리)

## 핫키

- **화살표 키**: 조각 이동/회전
- **Space**: 하드 드롭
- **F5**: 리플레이 기록 시작
- **F6**: 리플레이 중지 및 `out/replay.txt`에 저장
- **H**: 현재 상태 해시를 콘솔에 출력
- **R** (게임 오버): 재시작
- **Q** (게임 오버/취소): 타이틀 복귀/취소
