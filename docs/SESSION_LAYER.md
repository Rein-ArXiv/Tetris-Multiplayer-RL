# 세션 계층 (Session Layer)

Tetris-Multiplayer-RL 프로젝트의 세션 관리 및 Lockstep 동기화 시스템에 대한 완전한 가이드입니다.

## 📋 목차

1. [개념적 이해](#개념적-이해)
2. [Lockstep 동기화 알고리즘](#lockstep-동기화-알고리즘)
3. [세션 생명주기](#세션-생명주기)
4. [스레드 모델](#스레드-모델)
5. [API 참조](#api-참조)
6. [실제 사용 예제](#실제-사용-예제)
7. [디버깅 및 모니터링](#디버깅-및-모니터링)

---

## 개념적 이해

### Session 클래스의 역할

Session 클래스는 두 플레이어 간의 P2P(Peer-to-Peer) 연결을 관리하고, **결정론적 멀티플레이어 게임**을 위한 Lockstep 동기화를 구현합니다.

```cpp
// Session의 핵심 책임들
class Session {
    // 1. 연결 관리: TCP 핸드셰이크, 연결 상태 추적
    // 2. 동기화: Lockstep 알고리즘으로 입력 동기화
    // 3. 스레드 조정: Main/IO/Accept 스레드 간 협조
    // 4. 에러 복구: 연결 실패, 타임아웃 처리
};
```

### 결정론적 게임이란?

결정론적 게임은 **같은 입력 순서**가 주어지면 **항상 같은 결과**를 생성하는 게임입니다:

```
게임 상태 S₀ + 입력 I₁ → 게임 상태 S₁
게임 상태 S₁ + 입력 I₂ → 게임 상태 S₂
...

양쪽 플레이어가 동일한 S₀에서 시작하고
동일한 입력 순서 [I₁, I₂, I₃, ...]를 받으면
동일한 최종 상태에 도달함
```

이를 위해 필요한 조건들:
- **동일한 RNG 시드**: 랜덤 블록 순서가 같아야 함
- **동일한 입력 순서**: 모든 입력이 같은 순서로 적용되어야 함
- **동일한 게임 규칙**: 물리, 충돌 검사 등이 일치해야 함

---

## Lockstep 동기화 알고리즘

### 기본 원리

Lockstep은 **모든 플레이어의 입력을 기다린 후**에만 게임을 진행하는 동기화 방식입니다.

```
플레이어 A: [INPUT_A for tick N] ──┐
                                   ├─→ 두 입력 모두 도착 대기
플레이어 B: [INPUT_B for tick N] ──┘
                                   │
                                   ↓ 안전하게 진행 가능
                              게임 틱 N 실행
                              (A와 B의 입력 모두 적용)
```

### 안전 틱 계산

언제까지 게임을 진행할 수 있는지 계산하는 핵심 알고리즘:

```cpp
// main.cpp에서 실제 사용되는 계산 방식
int64_t lastLocalSent = localTickNext - 1;        // 내가 보낸 마지막 틱
int64_t lastRemote = session.maxRemoteTick();     // 상대방이 보낸 마지막 틱
int64_t safeTickInclusive = min(lastLocalSent, lastRemote) - inputDelay;

// safeTickInclusive까지는 안전하게 시뮬레이션 가능
while (simTick <= safeTickInclusive) {
    // 틱 simTick에 대한 양쪽 입력 모두 확보됨
    uint8_t localInput = getLocalInput(simTick);
    uint8_t remoteInput;
    session.GetRemoteInput(simTick, remoteInput);

    // 결정론적 게임 진행
    game.update(localInput, remoteInput);
    simTick++;
}
```

**계산 설명**:
- `lastLocalSent`: 내가 전송 완료한 마지막 틱
- `lastRemote`: 상대방으로부터 받은 마지막 틱
- `inputDelay`: 네트워크 지연을 흡수하기 위한 안전 버퍼
- `min()`: 둘 중 느린 쪽에 맞춤 (약한 고리 원칙)

### 입력 지연의 필요성

네트워크는 완벽하지 않습니다. 지연이 발생할 수 있습니다:

```
시나리오 1: 입력 지연 없음 (input_delay = 0)
┌─────────────────────────────────────────────────────────┐
│ 틱 100: A 입력 전송 ──→ [네트워크 지연] ──→ B 도착 지연 │
│ 틱 100: B 입력 전송 ──→ [정상] ──────────→ A 즉시 도착  │
│                                                         │
│ 결과: A는 B의 입력을 기다리며 게임 정지 (끊김 현상)      │
└─────────────────────────────────────────────────────────┘

시나리오 2: 입력 지연 있음 (input_delay = 2)
┌─────────────────────────────────────────────────────────┐
│ 틱 100: A 입력 전송 ──→ [지연 발생] ──→ B 틱 102에 도착 │
│ 틱 100: B 입력 전송 ──→ [정상] ─────→ A 틱 101에 도착   │
│                                                         │
│ 안전 틱 = min(100, 100) - 2 = 98                       │
│ 틱 98 실행: 이미 양쪽 입력 모두 도착함 (부드러운 진행)   │
└─────────────────────────────────────────────────────────┘
```

**지연 값 선택 가이드**:
- `0틱`: 지연 없음, 네트워크 지터 시 끊김
- `2틱`: ~33ms 지연, 일반적 인터넷 환경
- `4틱`: ~67ms 지연, 불안정한 연결용

---

## 세션 생명주기

### 연결 설정 프로세스

#### 호스트 모드 (Host)

```cpp
// 1. 호스트 시작
Session session;
SeedParams params{
    .seed = 0x123456789ABCDEF0,  // RNG 시드
    .start_tick = 120,           // 2초 준비 시간
    .input_delay = 2,            // 2틱 입력 지연
    .role = Role::Host
};

if (!session.Host(7777, params)) {
    std::cout << "호스트 시작 실패!" << std::endl;
    return;
}

// 2. 연결 대기
while (!session.isConnected()) {
    // UI: "연결 대기 중..." 표시
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// 3. 핸드셰이크 대기
while (!session.isReady()) {
    // UI: "핸드셰이크 진행 중..." 표시
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// 4. 게임 시작!
std::cout << "게임 준비 완료!" << std::endl;
```

#### 클라이언트 모드 (Peer)

```cpp
// 1. 클라이언트 연결
Session session;
if (!session.Connect("192.168.1.100", 7777)) {
    std::cout << "연결 실패!" << std::endl;
    return;
}

// 2. 핸드셰이크 대기 (SEED 메시지 수신)
while (!session.isReady()) {
    if (session.hasFailed()) {
        std::cout << "연결 시간 초과!" << std::endl;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// 3. 게임 파라미터 확인
SeedParams params = session.params();
std::cout << "게임 시작! 시드: 0x" << std::hex << params.seed << std::endl;
```

### 프로토콜 흐름도

```
Host (서버)                          Peer (클라이언트)
│                                   │
│ tcp_listen(7777)                 │ tcp_connect(host, 7777)
│ ←─────────────────────────────── │ (TCP 3-way 핸드셰이크)
│                                   │
│ ← HELLO (proto_ver=1) ─────────── │ (프로토콜 버전 확인)
│ ─ HELLO_ACK ─────────────────────→ │
│ ─ SEED (seed, start_tick...) ───→ │ (게임 파라미터 전달)
│                                   │ ready = true
│ ready = true                     │
│                                   │
│ ←────── 게임 INPUT ─────────────→ │ (Lockstep 게임 시작)
│ ←────── 게임 INPUT ─────────────→ │
│ ...                               ...
```

---

## 스레드 모델

Session 클래스는 최대 3개의 스레드를 사용합니다:

### Main Thread (게임 스레드)
```cpp
// 60 FPS 게임 루프에서 실행
while (gameRunning) {
    // 1. 사용자 입력 수집
    uint8_t localInput = collectInput();

    // 2. 네트워크로 전송
    session.SendInput(localTickNext, localInput);
    localTickNext++;

    // 3. 안전 틱까지 시뮬레이션
    int64_t safeTick = calculateSafeTick();
    while (simTick <= safeTick) {
        uint8_t remoteInput;
        if (session.GetRemoteInput(simTick, remoteInput)) {
            game.update(localInput, remoteInput);
        }
        simTick++;
    }

    // 4. 화면 렌더링
    game.render();

    // 60 FPS 유지
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```

### I/O Thread (네트워크 스레드)
```cpp
void Session::ioThread() {
    while (!quit.load()) {
        // 1. TCP에서 데이터 수신 (논블로킹)
        if (tcp_recv_some(sock, recvBuf)) {
            // 2. 프레임 파싱 및 처리
            std::vector<Frame> frames;
            parse_frames(recvBuf, frames);
            for (auto& frame : frames) {
                handleFrame(frame);  // INPUT 메시지 → remoteInputs 맵에 저장
            }
        }

        // 3. 송신 큐 비우기
        {
            std::lock_guard<std::mutex> lk(sendMu);
            while (!sendQ.empty()) {
                auto& packet = sendQ.front();
                tcp_send_all(sock, packet.data(), packet.size());
                sendQ.pop_front();
            }
        }

        // 4. CPU 절약
        if (!hasActivity) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}
```

### Accept Thread (호스트 전용)
```cpp
void Session::acceptThread(uint16_t port) {
    // 1. 대기 소켓 생성
    listenSock = tcp_listen(port, 1);

    // 2. 클라이언트 연결 대기 (블로킹)
    TcpSocket client = tcp_accept(listenSock);

    // 3. 연결 완료 처리
    sock = client;
    connected = true;

    // 4. I/O 스레드 시작
    th = std::thread(&Session::ioThread, this);

    // 5. 초기 메시지 전송
    sendHelloAndSeed();
    ready = true;
}
```

### 스레드 동기화

**Atomic 변수** (잠금 없는 상태 공유):
```cpp
std::atomic<bool> connected{false};      // TCP 연결 완료
std::atomic<bool> ready{false};          // 게임 시작 가능
std::atomic<bool> quit{false};           // 종료 신호
std::atomic<uint32_t> lastRemoteTick{0}; // 상대방 최신 틱
```

**Mutex 보호 영역** (복합 데이터 구조):
```cpp
std::mutex sendMu;  // 송신 큐 보호
std::deque<std::vector<uint8_t>> sendQ;

std::mutex inMu;    // 입력 맵 보호
std::unordered_map<uint32_t, uint8_t> remoteInputs;
```

---

## API 참조

### 생성자 및 소멸자

#### `Session()`
**목적**: 빈 세션 객체를 생성합니다.

```cpp
Session session;  // 모든 멤버가 기본값으로 초기화됨
```

**초기 상태**:
- 모든 atomic 변수는 `false` 또는 `0`
- 소켓은 무효 상태
- 스레드는 시작되지 않음

#### `~Session()`
**목적**: 세션을 안전하게 정리합니다.

```cpp
{
    Session session;
    // ... 게임 진행 ...
}  // 소멸자가 자동으로 Close() 호출
```

**정리 과정**:
1. `quit = true` 설정으로 모든 스레드 종료 신호
2. 대기 소켓 닫기로 `accept()` 블로킹 해제
3. 모든 스레드 조인으로 완전 종료 대기
4. 통신 소켓 정리

### 연결 설정 API

#### `bool Host(uint16_t port, const SeedParams& sp)`
**목적**: 호스트 모드로 세션을 시작하여 클라이언트 연결을 기다립니다.

**매개변수**:
- `port`: 대기할 TCP 포트 번호 (1024-65535)
- `sp`: 게임 시작 파라미터 (클라이언트에게 전송됨)

**반환값**:
- `true`: Accept 스레드 시작 성공
- `false`: 이미 대기 중이거나 리소스 부족

**사용 예제**:
```cpp
SeedParams hostParams{
    .seed = generateRandomSeed(),
    .start_tick = 120,      // 2초 준비 시간
    .input_delay = 2,       // 33ms 입력 지연
    .role = Role::Host
};

if (session.Host(7777, hostParams)) {
    std::cout << "포트 7777에서 대기 시작" << std::endl;

    // 연결 상태 모니터링
    while (!session.isConnected()) {
        if (shouldCancelWaiting()) {
            session.Close();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
} else {
    std::cout << "호스트 시작 실패" << std::endl;
}
```

**내부 동작**:
1. `listening = true` 설정
2. Accept 스레드 시작 (`acceptThread` 함수)
3. Accept 스레드가 `tcp_listen()` 및 `tcp_accept()` 수행
4. 클라이언트 연결 시 I/O 스레드 시작
5. HELLO 및 SEED 메시지 자동 전송

#### `bool Connect(const std::string& host, uint16_t port)`
**목적**: 클라이언트 모드로 지정된 호스트에 연결을 시도합니다.

**매개변수**:
- `host`: 호스트 주소 (IP 주소 또는 도메인명)
- `port`: 호스트 포트 번호

**반환값**:
- `true`: TCP 연결 성공 및 I/O 스레드 시작됨
- `false`: 연결 실패 (호스트 도달 불가, 포트 닫힘 등)

**사용 예제**:
```cpp
if (session.Connect("192.168.1.100", 7777)) {
    std::cout << "연결 성공" << std::endl;

    // 핸드셰이크 대기 (SEED 메시지 수신)
    auto startTime = std::chrono::steady_clock::now();
    while (!session.isReady()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(10)) {
            std::cout << "핸드셰이크 시간 초과" << std::endl;
            session.Close();
            return;
        }

        if (session.hasFailed()) {
            std::cout << "연결 실패" << std::endl;
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 게임 파라미터 확인
    SeedParams params = session.params();
    std::cout << "게임 준비! RNG 시드: 0x"
              << std::hex << params.seed << std::endl;
} else {
    std::cout << "연결 실패: " << host << ":" << port << std::endl;
}
```

**내부 동작**:
1. `tcp_connect()`로 즉시 TCP 연결 시도
2. 연결 성공 시 `connected = true` 설정
3. I/O 스레드 시작
4. HELLO 메시지 즉시 전송
5. SEED 메시지 수신 대기

### 상태 조회 API

#### `bool isConnected() const`
**목적**: TCP 연결이 완료되었는지 확인합니다.

**반환값**:
- `true`: TCP 연결 활성화 상태
- `false`: 연결되지 않음 또는 끊어짐

**사용 시점**: UI에서 연결 상태 표시용

```cpp
if (session.isConnected()) {
    DrawText("CONNECTED", 10, 10, 20, GREEN);
} else {
    DrawText("DISCONNECTED", 10, 10, 20, RED);
}
```

#### `bool isReady() const`
**목적**: 게임 시작 준비가 완료되었는지 확인합니다.

**반환값**:
- `true`: 핸드셰이크 완료, 게임 파라미터 협상 완료
- `false`: 아직 준비 중 또는 연결 실패

**사용 시점**: 게임 루프 시작 조건

```cpp
// 게임 시작 대기
while (!session.isReady() && !session.hasFailed()) {
    drawWaitingScreen();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

if (session.isReady()) {
    startGameLoop();
} else {
    showErrorScreen();
}
```

#### `bool isListening() const`
**목적**: 호스트 모드에서 연결 대기 중인지 확인합니다.

**반환값**:
- `true`: 호스트 모드로 클라이언트 연결 대기 중
- `false`: 클라이언트 모드이거나 연결 완료됨

#### `bool hasFailed() const`
**목적**: 연결 실패 또는 오류가 발생했는지 확인합니다.

**반환값**:
- `true`: 연결 시간 초과, 전송 실패 등의 오류 발생
- `false`: 정상 상태

**연결 실패 상황들**:
- 10초 핸드셰이크 시간 초과
- TCP 연결 끊어짐 감지
- 전송 실패 (EPIPE, ECONNRESET 등)

#### `SeedParams params() const`
**목적**: 협상된 게임 시작 파라미터를 조회합니다.

**반환값**: `SeedParams` 구조체 복사본

**사용 예제**:
```cpp
if (session.isReady()) {
    SeedParams p = session.params();

    // 게임 초기화
    gameRng.seed(p.seed);
    inputDelayTicks = p.input_delay;
    startCountdownTicks = p.start_tick;

    std::cout << "게임 파라미터:" << std::endl;
    std::cout << "  RNG 시드: 0x" << std::hex << p.seed << std::endl;
    std::cout << "  입력 지연: " << (int)p.input_delay << " 틱" << std::endl;
    std::cout << "  시작 대기: " << p.start_tick << " 틱" << std::endl;
    std::cout << "  내 역할: " << (p.role == Role::Host ? "Host" : "Peer") << std::endl;
}
```

### 데이터 송신 API

#### `void SendInput(uint32_t tick, uint8_t mask)`
**목적**: 지정된 틱의 플레이어 입력을 상대방에게 전송합니다.

**매개변수**:
- `tick`: 입력이 적용될 틱 번호 (순서대로 증가해야 함)
- `mask`: 입력 비트마스크 (`INPUT_LEFT | INPUT_ROTATE` 등)

**사용 패턴**:
```cpp
// 매 틱마다 호출 (입력이 없어도 0으로 전송)
while (gameRunning) {
    uint8_t currentInput = 0;

    // 입력 수집
    if (IsKeyPressed(KEY_LEFT))  currentInput |= INPUT_LEFT;
    if (IsKeyPressed(KEY_RIGHT)) currentInput |= INPUT_RIGHT;
    if (IsKeyPressed(KEY_DOWN))  currentInput |= INPUT_DOWN;
    if (IsKeyPressed(KEY_SPACE)) currentInput |= INPUT_ROTATE;
    if (IsKeyPressed(KEY_UP))    currentInput |= INPUT_DROP;

    // 네트워크 전송
    session.SendInput(localTickNext, currentInput);
    localTickNext++;

    // 게임 진행
    updateGame();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```

**입력 비트마스크 정의**:
```cpp
#define INPUT_NONE     0x00  // 입력 없음
#define INPUT_LEFT     0x01  // 왼쪽 이동
#define INPUT_RIGHT    0x02  // 오른쪽 이동
#define INPUT_DOWN     0x04  // 아래로 이동
#define INPUT_ROTATE   0x08  // 블록 회전
#define INPUT_DROP     0x10  // 하드 드롭
```

**내부 동작**:
1. `lastLocalTick` 업데이트 (안전 틱 계산용)
2. INPUT 메시지 생성 (`MsgType::INPUT`)
3. 송신 큐에 추가 (I/O 스레드가 비동기 전송)

#### `void SendHash(uint32_t tick, uint64_t hash)`
**목적**: 게임 상태 해시를 전송하여 동기화를 검증합니다.

**매개변수**:
- `tick`: 해시를 계산한 틱 번호
- `hash`: 게임 상태의 해시값 (CRC32, FNV 등)

**사용 예제**:
```cpp
// 1초마다 상태 검증
if (simTick % 60 == 0) {  // 60 FPS 기준
    uint64_t myHash = game.calculateStateHash();
    session.SendHash(simTick, myHash);

    // 상대방 해시와 비교
    uint32_t remoteTick;
    uint64_t remoteHash;
    if (session.GetLastRemoteHash(remoteTick, remoteHash)) {
        if (remoteTick == simTick && remoteHash != myHash) {
            std::cout << "경고: 게임 상태 불일치 감지!" << std::endl;
            std::cout << "틱 " << simTick << ": 내 해시=0x" << std::hex << myHash
                      << ", 상대방 해시=0x" << remoteHash << std::endl;
        }
    }
}
```

**게임 상태 해시 계산 예제**:
```cpp
uint64_t Game::calculateStateHash() const {
    uint64_t hash = 0x811C9DC5;  // FNV-1a 시작값

    // 보드 상태
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            hash ^= board[y][x];
            hash *= 0x01000193;
        }
    }

    // 현재 블록
    hash ^= currentPiece.type;
    hash *= 0x01000193;
    hash ^= currentPiece.x;
    hash *= 0x01000193;
    hash ^= currentPiece.y;
    hash *= 0x01000193;
    hash ^= currentPiece.rotation;
    hash *= 0x01000193;

    // 다음 블록들 (RNG 상태 반영)
    for (int i = 0; i < nextPieces.size() && i < 3; i++) {
        hash ^= nextPieces[i];
        hash *= 0x01000193;
    }

    return hash;
}
```

### 데이터 수신 API

#### `bool GetRemoteInput(uint32_t tick, uint8_t& outMask)`
**목적**: 지정된 틱의 상대방 입력을 조회합니다.

**매개변수**:
- `tick`: 조회할 틱 번호
- `outMask`: 입력 비트마스크 반환 변수

**반환값**:
- `true`: 해당 틱 입력이 도착함, `outMask`에 입력 저장됨
- `false`: 아직 입력이 도착하지 않음

**사용 패턴**:
```cpp
// Lockstep 게임 진행
while (simTick <= calculateSafeTick()) {
    uint8_t localInput = getStoredLocalInput(simTick);
    uint8_t remoteInput;

    if (session.GetRemoteInput(simTick, remoteInput)) {
        // 양쪽 입력 모두 준비됨 - 게임 진행
        game.update(localInput, remoteInput);
        simTick++;
    } else {
        // 상대방 입력 대기 - 이 틱은 건너뜀
        break;
    }
}
```

**입력 처리 예제**:
```cpp
void Game::update(uint8_t localInput, uint8_t remoteInput) {
    // 로컬 플레이어 입력 적용
    if (localInput & INPUT_LEFT)   localPlayer.moveLeft();
    if (localInput & INPUT_RIGHT)  localPlayer.moveRight();
    if (localInput & INPUT_DOWN)   localPlayer.moveDown();
    if (localInput & INPUT_ROTATE) localPlayer.rotate();
    if (localInput & INPUT_DROP)   localPlayer.hardDrop();

    // 원격 플레이어 입력 적용
    if (remoteInput & INPUT_LEFT)   remotePlayer.moveLeft();
    if (remoteInput & INPUT_RIGHT)  remotePlayer.moveRight();
    if (remoteInput & INPUT_DOWN)   remotePlayer.moveDown();
    if (remoteInput & INPUT_ROTATE) remotePlayer.rotate();
    if (remoteInput & INPUT_DROP)   remotePlayer.hardDrop();

    // 게임 물리 업데이트
    localPlayer.updatePhysics();
    remotePlayer.updatePhysics();
}
```

#### `bool GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const`
**목적**: 상대방이 보낸 가장 최근 상태 해시를 조회합니다.

**매개변수**:
- `tick`: 해시 틱 번호 반환 변수
- `hash`: 해시값 반환 변수

**반환값**:
- `true`: 유효한 해시 데이터 존재
- `false`: 아직 해시를 받지 못함

**사용 예제**:
```cpp
// 상태 동기화 검증
void verifyGameState() {
    uint32_t remoteTick;
    uint64_t remoteHash;

    if (session.GetLastRemoteHash(remoteTick, remoteHash)) {
        uint64_t myHash = game.calculateStateHash(remoteTick);

        if (myHash != remoteHash) {
            std::cout << "🚨 DESYNC 감지!" << std::endl;
            std::cout << "틱 " << remoteTick << std::endl;
            std::cout << "내 해시:   0x" << std::hex << myHash << std::endl;
            std::cout << "상대 해시: 0x" << std::hex << remoteHash << std::endl;

            // 디버깅을 위한 상태 덤프
            game.dumpState(remoteTick);
        } else {
            std::cout << "✅ 틱 " << remoteTick << " 동기화 검증 성공" << std::endl;
        }
    }
}
```

### 진행 상황 조회 API

#### `uint32_t maxRemoteTick() const`
**목적**: 상대방이 전송한 가장 높은 틱 번호를 반환합니다.

**반환값**: 상대방 최신 입력 틱 (0부터 시작)

**용도**: 안전 틱 계산에 사용

#### `uint32_t maxLocalTick() const`
**목적**: 내가 전송 완료한 가장 높은 틱 번호를 반환합니다.

**반환값**: 로컬 최신 전송 틱 (0부터 시작)

**안전 틱 계산 예제**:
```cpp
int64_t calculateSafeTick() {
    int64_t lastLocalSent = localTickNext - 1;  // 다음 전송 예정 - 1
    int64_t lastRemoteReceived = session.maxRemoteTick();
    int64_t inputDelay = session.params().input_delay;

    // 둘 중 느린 쪽에 맞추고, 입력 지연만큼 빼기
    int64_t safeTick = std::min(lastLocalSent, (int64_t)lastRemoteReceived) - inputDelay;

    // 음수 방지
    return std::max(safeTick, (int64_t)0);
}
```

### 세션 종료 API

#### `void Close()`
**목적**: 모든 네트워크 리소스를 정리하고 스레드를 안전하게 종료합니다.

**정리 순서**:
1. `quit = true` 설정 (모든 스레드 종료 신호)
2. 대기 소켓 닫기 (`tcp_accept()` 블로킹 해제)
3. Accept 스레드 조인 대기
4. I/O 스레드 조인 대기
5. 통신 소켓 닫기
6. 상태 변수 초기화

**사용 시점**:
```cpp
// 게임 종료 시
void gameShutdown() {
    std::cout << "게임 종료 중..." << std::endl;
    session.Close();  // 네트워크 정리
    std::cout << "네트워크 정리 완료" << std::endl;
}

// 에러 발생 시
if (session.hasFailed()) {
    std::cout << "연결 오류로 인한 강제 종료" << std::endl;
    session.Close();
}

// Ctrl+C 시그널 핸들러
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "사용자 중단 요청" << std::endl;
        session.Close();
        exit(0);
    }
}
```

**주의사항**:
- `Close()`는 블로킹 호출입니다 (스레드 조인 대기)
- 여러 번 호출해도 안전합니다
- 소멸자에서 자동 호출되므로 수동 호출은 선택사항입니다

---

## 실제 사용 예제

### 완전한 호스트 구현

```cpp
#include "session.h"
#include <iostream>
#include <chrono>
#include <random>

int main() {
    net::Session session;

    // 1. 게임 파라미터 설정
    std::random_device rd;
    std::mt19937_64 seedGen(rd());

    net::SeedParams hostParams{
        .seed = seedGen(),          // 랜덤 시드 생성
        .start_tick = 180,          // 3초 준비 시간
        .input_delay = 3,           // 50ms 입력 지연 (안정성 우선)
        .role = net::Role::Host
    };

    std::cout << "=== Tetris 멀티플레이어 호스트 ===" << std::endl;
    std::cout << "게임 시드: 0x" << std::hex << hostParams.seed << std::dec << std::endl;

    // 2. 호스트 시작
    if (!session.Host(7777, hostParams)) {
        std::cerr << "호스트 시작 실패!" << std::endl;
        return 1;
    }

    std::cout << "포트 7777에서 클라이언트 연결 대기 중..." << std::endl;

    // 3. 연결 대기 (타임아웃 포함)
    auto waitStart = std::chrono::steady_clock::now();
    const auto WAIT_TIMEOUT = std::chrono::minutes(5);

    while (!session.isConnected()) {
        auto elapsed = std::chrono::steady_clock::now() - waitStart;
        if (elapsed > WAIT_TIMEOUT) {
            std::cout << "연결 대기 시간 초과" << std::endl;
            session.Close();
            return 1;
        }

        std::cout << "대기 중... ("
                  << std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()
                  << "초)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "클라이언트 연결됨!" << std::endl;

    // 4. 핸드셰이크 대기
    while (!session.isReady()) {
        if (session.hasFailed()) {
            std::cout << "핸드셰이크 실패" << std::endl;
            session.Close();
            return 1;
        }
        std::cout << "핸드셰이크 진행 중..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "게임 준비 완료!" << std::endl;

    // 5. 게임 루프
    runGameLoop(session);

    // 6. 정리
    std::cout << "게임 종료" << std::endl;
    session.Close();
    return 0;
}

void runGameLoop(net::Session& session) {
    uint32_t localTick = 0;
    uint32_t simTick = 0;

    auto lastFrame = std::chrono::steady_clock::now();
    const auto FRAME_TIME = std::chrono::milliseconds(16);  // 60 FPS

    while (session.isConnected() && !session.hasFailed()) {
        auto now = std::chrono::steady_clock::now();

        // 1. 입력 수집 (실제 게임에서는 키보드 입력)
        uint8_t currentInput = 0;
        static int inputPattern = 0;

        // 테스트용 입력 패턴
        switch ((inputPattern / 60) % 4) {
            case 0: currentInput = INPUT_LEFT; break;
            case 1: currentInput = INPUT_RIGHT; break;
            case 2: currentInput = INPUT_ROTATE; break;
            case 3: currentInput = INPUT_DOWN; break;
        }
        inputPattern++;

        // 2. 입력 전송
        session.SendInput(localTick, currentInput);
        localTick++;

        // 3. 안전 틱 계산
        net::SeedParams params = session.params();
        int64_t lastLocalSent = localTick - 1;
        int64_t lastRemote = session.maxRemoteTick();
        int64_t safeTick = std::min(lastLocalSent, (int64_t)lastRemote) - params.input_delay;

        // 4. 게임 시뮬레이션
        while (simTick <= safeTick && simTick < 1000) {  // 최대 1000틱 테스트
            uint8_t remoteInput;
            if (session.GetRemoteInput(simTick, remoteInput)) {
                // 실제 게임에서는 여기서 게임 상태 업데이트
                std::cout << "틱 " << simTick
                          << ": 로컬=0x" << std::hex << currentInput
                          << ", 원격=0x" << remoteInput << std::dec << std::endl;

                // 10틱마다 상태 해시 전송
                if (simTick % 10 == 0) {
                    uint64_t fakeHash = simTick * 0x123456789ABCDEF0ULL;  // 가짜 해시
                    session.SendHash(simTick, fakeHash);
                }

                simTick++;
            } else {
                break;  // 상대방 입력 대기
            }
        }

        // 5. 게임 종료 조건
        if (simTick >= 1000) {
            std::cout << "테스트 완료 (1000틱 달성)" << std::endl;
            break;
        }

        // 6. 프레임 레이트 제한
        auto nextFrame = lastFrame + FRAME_TIME;
        std::this_thread::sleep_until(nextFrame);
        lastFrame = nextFrame;
    }
}
```

### 완전한 클라이언트 구현

```cpp
#include "session.h"
#include <iostream>

int main() {
    net::Session session;

    std::cout << "=== Tetris 멀티플레이어 클라이언트 ===" << std::endl;

    // 1. 호스트 연결
    std::string hostIP = "127.0.0.1";  // 로컬 테스트
    uint16_t hostPort = 7777;

    std::cout << "호스트 " << hostIP << ":" << hostPort << "에 연결 시도..." << std::endl;

    if (!session.Connect(hostIP, hostPort)) {
        std::cerr << "연결 실패!" << std::endl;
        return 1;
    }

    std::cout << "TCP 연결 성공!" << std::endl;

    // 2. 게임 파라미터 수신 대기
    auto handshakeStart = std::chrono::steady_clock::now();
    const auto HANDSHAKE_TIMEOUT = std::chrono::seconds(30);

    while (!session.isReady()) {
        if (session.hasFailed()) {
            std::cout << "연결 실패 또는 시간 초과" << std::endl;
            session.Close();
            return 1;
        }

        auto elapsed = std::chrono::steady_clock::now() - handshakeStart;
        if (elapsed > HANDSHAKE_TIMEOUT) {
            std::cout << "핸드셰이크 시간 초과" << std::endl;
            session.Close();
            return 1;
        }

        std::cout << "게임 파라미터 수신 대기..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // 3. 게임 파라미터 확인
    net::SeedParams params = session.params();
    std::cout << "게임 파라미터 수신 완료!" << std::endl;
    std::cout << "  RNG 시드: 0x" << std::hex << params.seed << std::dec << std::endl;
    std::cout << "  시작 지연: " << params.start_tick << " 틱" << std::endl;
    std::cout << "  입력 지연: " << (int)params.input_delay << " 틱" << std::endl;
    std::cout << "  내 역할: Peer" << std::endl;

    // 4. 게임 루프 (호스트와 동일한 로직)
    runGameLoop(session);  // 위의 함수 재사용

    // 5. 정리
    std::cout << "게임 종료" << std::endl;
    session.Close();
    return 0;
}
```

---

## 디버깅 및 모니터링

### 연결 상태 실시간 모니터링

```cpp
void drawNetworkStatus(const net::Session& session) {
    // 연결 상태
    if (session.isConnected()) {
        DrawText("연결: 활성", 10, 10, 20, GREEN);
    } else {
        DrawText("연결: 끊어짐", 10, 10, 20, RED);
    }

    // 게임 준비 상태
    if (session.isReady()) {
        DrawText("상태: 게임 중", 10, 35, 20, GREEN);
    } else if (session.isListening()) {
        DrawText("상태: 대기 중", 10, 35, 20, YELLOW);
    } else {
        DrawText("상태: 연결 중", 10, 35, 20, YELLOW);
    }

    // 틱 정보
    std::string tickInfo = "틱 - 로컬: " + std::to_string(session.maxLocalTick()) +
                          ", 원격: " + std::to_string(session.maxRemoteTick());
    DrawText(tickInfo.c_str(), 10, 60, 16, WHITE);

    // 연결 실패 경고
    if (session.hasFailed()) {
        DrawText("경고: 연결 오류 발생!", 10, 85, 20, RED);
    }
}
```

### 성능 모니터링

```cpp
class NetworkProfiler {
private:
    struct Stats {
        size_t messagesSent = 0;
        size_t messagesReceived = 0;
        size_t bytesSent = 0;
        size_t bytesReceived = 0;
        std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    } stats;

public:
    void recordSent(size_t bytes) {
        stats.messagesSent++;
        stats.bytesSent += bytes;
    }

    void recordReceived(size_t bytes) {
        stats.messagesReceived++;
        stats.bytesReceived += bytes;
    }

    void printStats() const {
        auto elapsed = std::chrono::steady_clock::now() - stats.startTime;
        auto seconds = std::chrono::duration<double>(elapsed).count();

        std::cout << "=== 네트워크 통계 (" << seconds << "초) ===" << std::endl;
        std::cout << "송신: " << stats.messagesSent << " 메시지, "
                  << stats.bytesSent << " 바이트" << std::endl;
        std::cout << "수신: " << stats.messagesReceived << " 메시지, "
                  << stats.bytesReceived << " 바이트" << std::endl;

        if (seconds > 0) {
            std::cout << "송신 속도: " << (stats.bytesSent / seconds / 1024) << " KB/s" << std::endl;
            std::cout << "수신 속도: " << (stats.bytesReceived / seconds / 1024) << " KB/s" << std::endl;
        }
    }
};
```

### Lockstep 지연 분석

```cpp
class LockstepAnalyzer {
private:
    std::vector<int64_t> waitTimes;  // 입력 대기 시간들
    std::chrono::steady_clock::time_point lastWaitStart;
    bool isWaiting = false;

public:
    void startWaiting() {
        if (!isWaiting) {
            lastWaitStart = std::chrono::steady_clock::now();
            isWaiting = true;
        }
    }

    void endWaiting() {
        if (isWaiting) {
            auto elapsed = std::chrono::steady_clock::now() - lastWaitStart;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            waitTimes.push_back(ms);
            isWaiting = false;

            // 최근 100개 기록만 유지
            if (waitTimes.size() > 100) {
                waitTimes.erase(waitTimes.begin());
            }
        }
    }

    void printAnalysis() const {
        if (waitTimes.empty()) {
            std::cout << "대기 시간 데이터 없음" << std::endl;
            return;
        }

        int64_t sum = 0;
        int64_t maxWait = 0;
        for (auto wait : waitTimes) {
            sum += wait;
            maxWait = std::max(maxWait, wait);
        }

        double avgWait = (double)sum / waitTimes.size();

        std::cout << "=== Lockstep 지연 분석 ===" << std::endl;
        std::cout << "평균 대기: " << avgWait << " ms" << std::endl;
        std::cout << "최대 대기: " << maxWait << " ms" << std::endl;
        std::cout << "샘플 수: " << waitTimes.size() << std::endl;

        // 지연 빈도 분포
        int ranges[5] = {0}; // 0-10ms, 10-50ms, 50-100ms, 100-500ms, 500ms+
        for (auto wait : waitTimes) {
            if (wait < 10) ranges[0]++;
            else if (wait < 50) ranges[1]++;
            else if (wait < 100) ranges[2]++;
            else if (wait < 500) ranges[3]++;
            else ranges[4]++;
        }

        std::cout << "지연 분포:" << std::endl;
        std::cout << "  0-10ms:   " << ranges[0] << " (" << (100*ranges[0]/waitTimes.size()) << "%)" << std::endl;
        std::cout << "  10-50ms:  " << ranges[1] << " (" << (100*ranges[1]/waitTimes.size()) << "%)" << std::endl;
        std::cout << "  50-100ms: " << ranges[2] << " (" << (100*ranges[2]/waitTimes.size()) << "%)" << std::endl;
        std::cout << "  100-500ms:" << ranges[3] << " (" << (100*ranges[3]/waitTimes.size()) << "%)" << std::endl;
        std::cout << "  500ms+:   " << ranges[4] << " (" << (100*ranges[4]/waitTimes.size()) << "%)" << std::endl;
    }
};

// 사용 예제
LockstepAnalyzer analyzer;

while (gameRunning) {
    // 안전 틱 계산
    int64_t safeTick = calculateSafeTick();

    if (simTick > safeTick) {
        analyzer.startWaiting();  // 대기 시작
        // 입력을 기다리는 중...
    } else {
        analyzer.endWaiting();    // 대기 끝
        // 게임 진행
        simulateGameTick();
    }
}

analyzer.printAnalysis();  // 게임 종료 시 분석 출력
```

이 문서는 Session 클래스의 모든 기능과 Lockstep 동기화 알고리즘을 상세히 다룹니다. 실제 게임 개발 시 이 가이드를 참조하여 안정적인 멀티플레이어 시스템을 구축할 수 있습니다.