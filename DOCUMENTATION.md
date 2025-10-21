# Tetris Multiplayer RL - 완전 문서

## 프로젝트 개요

C++와 Raylib로 구현한 P2P Lockstep 네트워크 멀티플레이어 테트리스.

**핵심 특징**:
- 결정론적 시뮬레이션 (고정 60Hz 틱)
- P2P Lockstep 동기화
- 플랫폼 독립적 네트워킹 (Windows/Linux)
- 리플레이 시스템

---

## 빌드 및 실행

### Windows (w64devkit + raylib)

```bash
mingw32-make PLATFORM=PLATFORM_DESKTOP RAYLIB_PATH=C:/raylib/raylib
```

**요구사항**:
- raylib: `C:/raylib`
- w64devkit: `C:/raylib/w64devkit/w64devkit.exe`
- 런타임 DLL: `lib/libstdc++-6.dll`, `lib/libgcc_s_dw2-1.dll`

### Linux/Mac (CMake)

```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j
./tetris
```

**요구사항**: raylib (pkg-config 또는 `/usr/local`)

---

## 아키텍처

### 계층 구조

```
Application (main.cpp)
    ↓
Session (net/session.*)     - Lockstep 동기화, 핸드셰이크
    ↓
Framing (net/framing.*)     - 메시지 직렬화 (LEN+TYPE+PAYLOAD+CHECKSUM)
    ↓
Socket (net/socket.*)       - TCP 추상화 (WinSock/BSD)
```

### 스레드 모델

- **메인 스레드**: 렌더링, 오디오, 입력 샘플링, 게임 시뮬레이션
- **I/O 스레드**: 논블로킹 소켓 수신/송신, 메시지 파싱
- **Accept 스레드** (호스트만): 클라이언트 연결 대기

**동기화**: 뮤텍스 보호 큐 + atomic 플래그

---

## 네트워크 계층

### Socket Layer (net/socket.*)

플랫폼 독립적 TCP 래퍼.

**주요 함수**:
- `tcp_listen(port)` - 서버 소켓 생성
- `tcp_accept(server)` - 클라이언트 연결 수락
- `tcp_connect(host, port)` - 서버 연결
- `tcp_send_all(sock, data, len)` - 전체 데이터 송신
- `tcp_recv_some(sock, buf)` - 논블로킹 수신
- `tcp_close(sock)` - 소켓 종료

**특징**:
- 논블로킹 I/O 설정
- Windows WinSock / Linux BSD 소켓 통합
- `SO_REUSEADDR` 포트 재사용

### Framing Layer (net/framing.*)

메시지 경계 구분 프로토콜.

**프레임 구조** (리틀엔디안):
```
┌────────┬────────┬─────────────┬──────────┐
│ LEN    │ TYPE   │ PAYLOAD     │ CHECKSUM │
│ 2bytes │ 1byte  │ LEN-1 bytes │ 4bytes   │
└────────┴────────┴─────────────┴──────────┘
```

**메시지 타입** (MsgType):
1. `HELLO` - 프로토콜 버전 교환
2. `HELLO_ACK` - HELLO 응답
3. `SEED` - 게임 파라미터 (seed, start_tick, input_delay, role)
4. `INPUT` - 틱별 입력 비트마스크
5. `ACK` - 수신 확인
6. `PING` / `PONG` - Keepalive
8. `HASH` - 상태 해시 (desync 감지)
9. `GAME_OVER_CHOICE` - 게임 오버 후 선택

**체크섬**: FNV-1a 32-bit 해시

### Session Layer (net/session.*)

P2P Lockstep 동기화 프로토콜.

**핸드셰이크 (호스트)**:
```cpp
session.Host(port, SeedParams{seed, start_tick, input_delay, Role::Host});
// → 포트 바인딩 → 클라이언트 대기 → HELLO + SEED 전송
```

**핸드셰이크 (클라이언트)**:
```cpp
session.Connect(host_ip, port);
// → 호스트 연결 → HELLO 전송 → SEED 수신
```

**게임 루프**:
```cpp
// 입력 전송
session.SendInput(localTick, inputMask);

// 상대 입력 대기 및 시뮬레이션
uint8_t remoteInput;
if (session.GetRemoteInput(simTick, remoteInput)) {
    game.SubmitInput(localInput | remoteInput);
    game.Tick();
    simTick++;
}
```

**Lockstep 안전 틱**:
```
safe_tick = min(local_sent_tick, remote_max_tick) - input_delay
```

**파라미터**:
- `inputDelay` (기본 2틱): 네트워크 지연 버퍼
- `startDelay` (기본 120틱): 연결 안정화 대기 시간

---

## 결정론 시스템

### 고정 틱 시스템

- **속도**: 60Hz (core/constants.h의 `TICKS_PER_SECOND`)
- **분리**: 로직(틱 기반) ≠ 렌더링(프레임 기반)
- **진행**: `Game::Tick()` 호출로만 상태 변경

### RNG (core/rng.h)

**XorShift64*** 알고리즘 - 결정론적 난수 생성.

```cpp
XorShift64Star rng(sessionSeed);
uint32_t blockType = rng.nextUInt(7);  // 0-6
```

**주의**: 모든 피어가 동일한 시드와 호출 순서 필요.

### 입력 시스템 (core/input.h)

**비트마스크** (5비트):
```cpp
INPUT_LEFT   = 0x01
INPUT_RIGHT  = 0x02
INPUT_DOWN   = 0x04
INPUT_ROTATE = 0x08
INPUT_DROP   = 0x10
```

**적용**:
```cpp
Game::SubmitInput(uint8_t mask);
```

### 상태 해싱 (core/hash.h)

**FNV-1a 64-bit** - desync 감지용 상태 지문.

```cpp
uint64_t hash = game.ComputeStateHash();
session.SendHash(tick, hash);
```

**해시 포함**: 그리드, 블록, RNG 상태, 점수, 중력 카운터

### 리플레이 시스템 (core/replay.*)

**형식**: 텍스트 (시드 + 틱별 입력)
```
SEED 12345678
0 0x00
1 0x02
2 0x02
3 0x08
```

**사용**:
- F5: 기록 시작
- F6: `out/replay.txt` 저장

**목적**: 비결정론 버그 재현 및 디버깅

---

## 게임 오버 프로토콜

### 상태 머신

```
게임 오버 발생
    ↓
ShowingGameOver (선택 대기)
    ↓
WaitingForRemote (상대 응답 대기, 30초 타임아웃)
    ↓
┌──────────────┬──────────────┬──────────────┐
│ 양쪽 Restart │ 양쪽 Title   │ 불일치       │
└──────────────┴──────────────┴──────────────┘
      ↓              ↓              ↓
RestartingGame  GoingToTitle  ShowingDisagreement
                                (3초 카운트다운)
                                      ↓
                                  GoingToTitle
```

### 시나리오

1. **양쪽 Restart**: 새 시드로 게임 재시작
2. **양쪽 Title**: 메뉴로 복귀 (연결 유지)
3. **불일치**: "CHOICES DIFFER" 메시지 → 3초 후 타이틀
4. **타임아웃**: 30초 무응답 → "Connection timeout" → 타이틀

### 구현 (main.cpp)

```cpp
enum class GameOverState {
    None,
    ShowingGameOver,      // [R] Restart / [ESC] Go to Title
    WaitingForRemote,     // "Waiting for opponent..."
    ShowingDisagreement,  // 3초 카운트다운
    RestartingGame,       // 호스트가 새 시드 전송
    GoingToTitle,         // 세션 종료 및 메뉴 복귀
};
```

**메시지 전송**:
```cpp
session.SendGameOverChoice(GameOverChoice::Restart);
```

**응답 수신**:
```cpp
GameOverChoice remoteChoice;
if (session.GetRemoteGameOverChoice(remoteChoice)) {
    // 선택 비교 및 상태 전환
}
```

---

## 코드 구조

```
src/              UI, 렌더링, 오디오 (raylib)
  ├─ main.cpp     진입점, 메뉴, 네트워크 CLI, 듀얼 보드 렌더링
  ├─ game.*       Game 클래스: 보드 로직, 틱 진행
  ├─ grid.*       그리드 상태
  ├─ block.*      테트로미노 모양과 회전
  └─ colors.*     비주얼 팔레트

core/             결정론적 시뮬레이션 (순수 C++)
  ├─ constants.h  TICKS_PER_SECOND
  ├─ input.h      입력 비트마스크
  ├─ rng.*        XorShift64* RNG
  ├─ hash.h       FNV-1a 해싱
  └─ replay.*     리플레이 기록/재생

net/              네트워킹 계층
  ├─ socket.*     TCP 래퍼 (Windows/Linux)
  ├─ framing.*    메시지 직렬화
  └─ session.*    P2P Lockstep 프로토콜

Font/, Sounds/    에셋
```

**참고**: `src/blocks.cpp`는 `game.h`에 포함되며 별도 컴파일 안됨.

---

## 멀티플레이어 실행

### CLI 인자

```bash
# 호스트
./tetris --host 7777

# 클라이언트
./tetris --connect 192.168.1.100:7777
```

### HUD 정보

화면 하단:
- **NET**: CONNECTED/DISCONNECTED
- **TICKS**: `localSent=N remoteMax=M sim=K`
  - localSent: 피어에 전송한 마지막 틱
  - remoteMax: 피어로부터 받은 마지막 틱
  - sim: 시뮬레이션된 마지막 틱

### 듀얼 보드

네트워크 게임 시 로컬(좌) / 원격(우) 보드 나란히 표시.

---

## 개발 가이드

### 게임 로직 수정

1. **결정론 유지**: 모든 랜덤성은 `rng` 사용, 시스템 시간 금지
2. **해시 테스트**: `H` 키로 상태 해시 일치 확인
3. **리플레이**: F5/F6로 버그 재현

### 네트워킹 수정

1. **계층 분리**: socket/framing/session 관심사 독립 유지
2. **스레드 안전성**: 공유 상태는 뮤텍스/atomic 필수
3. **프로토콜 버전**: 프레임 형식 변경 시 HELLO 버전 업데이트

### Desync 디버깅

1. `H` 키로 해시 로깅
2. 양쪽 콘솔 출력 비교
3. 리플레이로 분기점 격리
4. 비결정론 소스 확인 (시스템 호출, 초기화 안된 메모리, 부동소수점)

### 일반 작업

**새 입력 추가**:
1. `core/input.h`에 비트 정의
2. `main.cpp` `SampleInput()` 수정
3. `Game::SubmitInput()` 처리

**틱 속도 변경**:
1. `core/constants.h` `TICKS_PER_SECOND` 수정
2. `inputDelay`, `startDelay` 비례 조정
3. 전체 재컴파일

---

## 플랫폼 노트

### Windows
- WinSock2 (`ws2_32`)
- `net::net_init()` / `net::net_shutdown()`
- MinGW g++ C++17

### Linux
- BSD 소켓
- `-lpthread -ldl -lrt`
- X11/Wayland (raylib)

---

## 핫키

- **화살표**: 이동/회전
- **Space**: 하드 드롭
- **F5**: 리플레이 기록 시작
- **F6**: 리플레이 저장 (`out/replay.txt`)
- **H**: 상태 해시 출력
- **R** (게임 오버): 재시작 선택
- **Q** (게임 오버/취소): 타이틀 복귀/취소

---

## 최적화 로드맵

- 패킷 배치: 60Hz → 20-30Hz
- 입력 압축: 런-렝스 인코딩
- 재연결: 타임아웃 처리 및 스냅샷 복구
- UDP 옵션: 커스텀 신뢰성 계층
