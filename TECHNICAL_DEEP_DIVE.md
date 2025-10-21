# Tetris Multiplayer - P2P Lockstep 네트워킹 구현 심층 분석

## 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [아키텍처 설계 철학](#2-아키텍처-설계-철학)
3. [결정론적 시뮬레이션 시스템](#3-결정론적-시뮬레이션-시스템)
4. [네트워킹 레이어 구현](#4-네트워킹-레이어-구현)
5. [게임 로직과 상태 관리](#5-게임-로직과-상태-관리)
6. [멀티플레이어 통합 과정](#6-멀티플레이어-통합-과정)
7. [직면한 문제와 해결책](#7-직면한-문제와-해결책)
8. [성능 최적화와 디버깅](#8-성능-최적화와-디버깅)
9. [배운 점과 개선 방향](#9-배운-점과-개선-방향)

---

## 1. 프로젝트 개요

### 1.1 목표

C++와 Raylib를 사용하여 **결정론적 P2P Lockstep 방식**의 멀티플레이어 테트리스를 구현한다. 외부 네트워킹 라이브러리 없이 TCP 소켓부터 직접 구현하여 네트워킹 기초를 학습한다.

### 1.2 핵심 요구사항

- **결정론(Determinism)**: 같은 입력 시퀀스 → 같은 게임 결과
- **Lockstep 동기화**: 모든 입력 도착 전까지 시뮬레이션 대기
- **리플레이 시스템**: 게임 세션 재현 가능
- **플랫폼 독립성**: Windows/Linux 모두 지원

### 1.3 기술 스택

```
언어: C++17
그래픽: Raylib 5.0
빌드: MinGW (Windows), CMake (Linux)
네트워킹: Raw TCP Sockets (WinSock2 / BSD Sockets)
```

### 1.4 프로젝트 구조

```
src/              # 게임 UI와 로직
  ├─ main.cpp     # 진입점, 메뉴, 네트워크 통합
  ├─ game.*       # Game 클래스 (보드 시뮬레이션)
  ├─ grid.*       # 그리드 상태 관리
  ├─ block.*      # 테트로미노 정의
  └─ colors.*     # 색상 팔레트

core/             # 결정론적 시뮬레이션 (플랫폼 독립)
  ├─ constants.h  # TICKS_PER_SECOND 정의
  ├─ input.h      # 입력 비트마스크
  ├─ rng.*        # XorShift64* RNG
  ├─ hash.h       # FNV-1a 상태 해싱
  └─ replay.*     # 리플레이 저장/로드

net/              # 네트워킹 3계층
  ├─ socket.*     # TCP 래퍼 (플랫폼 추상화)
  ├─ framing.*    # 메시지 직렬화 (LEN+TYPE+PAYLOAD+CHECKSUM)
  └─ session.*    # P2P Lockstep 프로토콜
```

---

## 2. 아키텍처 설계 철학

### 2.1 계층 분리 (Separation of Concerns)

프로젝트는 명확한 계층 구조를 따른다:

```
┌─────────────────────────────────────┐
│   Application Layer (main.cpp)     │ ← UI, 메뉴, 사용자 입력
├─────────────────────────────────────┤
│   Game Logic (src/game.*)           │ ← 테트리스 규칙, 보드 상태
├─────────────────────────────────────┤
│   Deterministic Core (core/)        │ ← 결정론 보장 (RNG, 해싱)
├─────────────────────────────────────┤
│   Session Layer (net/session.*)     │ ← Lockstep 동기화
├─────────────────────────────────────┤
│   Framing Layer (net/framing.*)     │ ← 메시지 직렬화
├─────────────────────────────────────┤
│   Socket Layer (net/socket.*)       │ ← TCP 추상화
└─────────────────────────────────────┘
```

**왜 이렇게 분리했는가?**

1. **테스트 용이성**: 각 레이어를 독립적으로 테스트 가능
2. **이식성**: `core/`는 플랫폼 독립적 (임베디드에서도 동작 가능)
3. **확장성**: 네트워킹 계층 교체 시 게임 로직은 불변
4. **디버깅**: 문제 발생 시 레이어별로 격리 가능

### 2.2 결정론 우선 설계

멀티플레이어에서 상태 동기화를 위해 두 가지 방식이 있다:

**A. State Synchronization** (일반적인 방식)
```
서버: 권위적 게임 상태 유지
클라이언트: 서버에서 상태를 받아 렌더링
문제: 대역폭 높음, 서버 필수
```

**B. Deterministic Lockstep** (본 프로젝트)
```
양쪽: 동일한 초기 상태 (seed)
양쪽: 동일한 입력 시퀀스 적용
결과: 동일한 최종 상태 (검증: 해시)
장점: 대역폭 낮음 (입력만 전송), P2P 가능
```

**결정론을 보장하기 위한 규칙:**

```cpp
// 금지: 시스템 시간 의존
❌ int nextBlock = rand() % 7;  // stdlib rand는 비결정론적
❌ uint64_t timestamp = time(NULL);

// 허용: 결정론적 RNG
✅ XorShift64Star rng(seed);
✅ int nextBlock = rng.nextUInt(7);
```

### 2.3 Fixed Timestep 시뮬레이션

게임 로직을 프레임률(FPS)과 분리한다:

```cpp
// 메인 루프 (main.cpp:146)
float accumulator = 0.0f;
const float SECONDS_PER_TICK = 1.0f / 60.0f;  // 60Hz

while (!WindowShouldClose()) {
    float deltaTime = GetFrameTime();  // 프레임 시간 (가변)
    accumulator += deltaTime;

    // 고정 틱으로 시뮬레이션 (60Hz 고정)
    while (accumulator >= SECONDS_PER_TICK) {
        game.Tick();  // 1/60초 진행
        accumulator -= SECONDS_PER_TICK;
    }

    // 렌더링 (FPS 무관)
    game.Draw();
}
```

**왜 Fixed Timestep인가?**

1. **결정론 보장**: 프레임률에 관계없이 동일한 결과
2. **물리 안정성**: 중력, 낙하 속도가 일정
3. **네트워크 동기화**: "틱 123"으로 명확히 식별 가능

---

## 3. 결정론적 시뮬레이션 시스템

### 3.1 난수 생성기 (core/rng.h)

**문제**: stdlib의 `rand()`는 플랫폼마다 다르고, 멀티스레드에서 안전하지 않다.

**해결**: XorShift64* 알고리즘 구현

```cpp
// core/rng.h
class XorShift64Star {
    uint64_t state;

public:
    explicit XorShift64Star(uint64_t seed) : state(seed ? seed : 0xBADC0FFEE0DDF00Dull) {}

    uint64_t next() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1Dull;
    }

    uint32_t nextUInt(uint32_t max) {
        return static_cast<uint32_t>(next() % max);
    }
};
```

**왜 XorShift64*인가?**

- 빠름: 곱셈 1회, XOR 3회만 필요
- 결정론적: 동일한 seed → 동일한 시퀀스
- 주기 긺: 2^64 - 1 (테트리스에 충분)
- 플랫폼 독립적: 정수 연산만 사용

**사용 예시:**

```cpp
// Game 클래스 초기화
Game::Game(uint64_t seed) : rng(seed) {
    SpawnNewBlock();  // RNG 사용
}

void Game::SpawnNewBlock() {
    currentBlock.type = static_cast<BlockType>(rng.nextUInt(7));
    // 양쪽 피어가 같은 seed면 같은 블록 생성
}
```

### 3.2 입력 시스템 (core/input.h)

입력을 비트마스크로 표현하여 네트워크 전송을 최소화한다:

```cpp
// core/input.h
constexpr uint8_t INPUT_NONE   = 0x00;
constexpr uint8_t INPUT_LEFT   = 0x01;  // 0000 0001
constexpr uint8_t INPUT_RIGHT  = 0x02;  // 0000 0010
constexpr uint8_t INPUT_DOWN   = 0x04;  // 0000 0100
constexpr uint8_t INPUT_ROTATE = 0x08;  // 0000 1000
constexpr uint8_t INPUT_DROP   = 0x10;  // 0001 0000
```

**왜 비트마스크인가?**

1. **압축**: 5개 버튼 → 1바이트 (vs 5바이트 bool 배열)
2. **결합**: `LEFT | DOWN` = 대각선 입력
3. **네트워크 효율**: 틱당 1바이트만 전송

**입력 샘플링 (main.cpp:20-29):**

```cpp
static uint8_t SampleInput() {
    uint8_t mask = INPUT_NONE;
    if (IsKeyPressed(KEY_LEFT))  mask |= INPUT_LEFT;
    if (IsKeyPressed(KEY_RIGHT)) mask |= INPUT_RIGHT;
    if (IsKeyDown(KEY_DOWN))     mask |= INPUT_DOWN;    // Hold
    if (IsKeyPressed(KEY_UP))    mask |= INPUT_ROTATE;
    if (IsKeyPressed(KEY_SPACE)) mask |= INPUT_DROP;
    return mask;
}
```

**주의: Pressed vs Down**

- `IsKeyPressed`: 프레임당 1회만 true (눌린 순간)
- `IsKeyDown`: 누르고 있는 동안 계속 true

**적용 (Game::SubmitInput):**

```cpp
void Game::SubmitInput(uint8_t mask) {
    if (mask & INPUT_LEFT)   MoveLeft();
    if (mask & INPUT_RIGHT)  MoveRight();
    if (mask & INPUT_DOWN)   MoveDown();
    if (mask & INPUT_ROTATE) Rotate();
    if (mask & INPUT_DROP)   HardDrop();
}
```

### 3.3 상태 해싱 (core/hash.h)

Desync 감지를 위해 게임 상태의 지문을 생성한다:

```cpp
// core/hash.h - FNV-1a 64-bit 해시
inline uint64_t fnv1a_64(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = 0xCBF29CE484222325ull;  // FNV offset basis
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001B3ull;  // FNV prime
    }
    return hash;
}
```

**Game::ComputeStateHash() 구현:**

```cpp
uint64_t Game::ComputeStateHash() const {
    uint64_t h = 0;

    // 그리드 상태 해싱
    h ^= fnv1a_64(grid.cells.data(), grid.cells.size());

    // 현재 블록 해싱
    h ^= fnv1a_64(&currentBlock, sizeof(currentBlock));

    // RNG 상태 해싱 (중요!)
    h ^= fnv1a_64(&rng.state, sizeof(rng.state));

    // 점수, 중력 카운터 등
    h ^= fnv1a_64(&score, sizeof(score));
    h ^= fnv1a_64(&gravityCounter, sizeof(gravityCounter));

    return h;
}
```

**사용 예시 (디버깅):**

```cpp
// main.cpp: H 키 누르면 해시 출력
if (IsKeyPressed(KEY_H)) {
    uint64_t localHash = gameLocal->ComputeStateHash();
    uint64_t remoteHash = gameRemote->ComputeStateHash();
    std::cout << "Local: 0x" << std::hex << localHash
              << " Remote: 0x" << remoteHash << std::dec << std::endl;

    if (localHash != remoteHash) {
        std::cerr << "DESYNC DETECTED!" << std::endl;
    }
}
```

### 3.4 리플레이 시스템 (core/replay.*)

게임 세션을 재현하기 위한 최소 데이터 저장:

```cpp
// core/replay.h
struct FrameInputs {
    uint8_t p1;  // Player 1 입력
    uint8_t p2;  // Player 2 입력 (미사용)
};

struct ReplayData {
    uint64_t seed;
    std::vector<FrameInputs> frames;
};
```

**저장 형식 (텍스트):**

```
SEED 12345678ABCDEF00
0 0x00
1 0x02
2 0x02
3 0x08
4 0x10
```

**저장 (main.cpp:302-308):**

```cpp
if (IsKeyPressed(KEY_F5)) {
    recording = true;
    replay.frames.clear();
}

if (recording) {
    FrameInputs fr{};
    fr.p1 = inputMask;
    fr.p2 = 0;
    replay.frames.push_back(fr);
}

if (IsKeyPressed(KEY_F6)) {
    ReplayIO::Save("out/replay.txt", replay);
    recording = false;
}
```

**재생:**

```cpp
ReplayData replay = ReplayIO::Load("out/replay.txt");
Game game(replay.seed);

for (const auto& frame : replay.frames) {
    game.SubmitInput(frame.p1);
    game.Tick();
}

// 동일한 최종 상태 보장
```

**왜 리플레이가 중요한가?**

1. **버그 재현**: 비결정론 문제 디버깅
2. **학습 데이터**: AI 강화학습용 데이터셋
3. **검증**: 코드 변경 후 회귀 테스트

---

## 4. 네트워킹 레이어 구현

### 4.1 Socket Layer (net/socket.*)

플랫폼 독립적인 TCP 래퍼를 구현한다.

**설계 목표:**

- Windows (WinSock2)와 Linux (BSD Sockets) 통합
- 논블로킹 I/O 지원
- 명확한 에러 처리

**구조체 정의:**

```cpp
// net/socket.h
namespace net {

struct TcpSocket {
    int fd{-1};  // 파일 디스크립터 (Windows에서는 SOCKET)
    bool valid() const { return fd >= 0; }
};

// 초기화/종료
bool net_init();      // WSAStartup (Windows) / no-op (Linux)
void net_shutdown();  // WSACleanup (Windows) / no-op (Linux)

// 서버
TcpSocket tcp_listen(uint16_t port, int backlog = 1);
TcpSocket tcp_accept(const TcpSocket& server);

// 클라이언트
TcpSocket tcp_connect(const std::string& host, uint16_t port);

// 송수신
bool tcp_send_all(const TcpSocket& sock, const void* data, size_t len);
bool tcp_recv_some(const TcpSocket& sock, std::vector<uint8_t>& buf);

// 종료
void tcp_close(TcpSocket& sock);

}  // namespace net
```

**tcp_listen 구현 분석:**

```cpp
TcpSocket tcp_listen(uint16_t port, int backlog) {
    // 1. 소켓 생성
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return TcpSocket{-1};

    // 2. SO_REUSEADDR 설정 (포트 재사용 허용)
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 주소 바인딩
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 모든 인터페이스
    addr.sin_port = htons(port);        // 네트워크 바이트 오더

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        tcp_close(TcpSocket{fd});
        return TcpSocket{-1};
    }

    // 4. 리스닝 시작
    if (listen(fd, backlog) < 0) {
        tcp_close(TcpSocket{fd});
        return TcpSocket{-1};
    }

    // 5. 논블로킹 설정
    set_nonblocking(fd);

    return TcpSocket{fd};
}
```

**왜 SO_REUSEADDR?**

포트를 재사용하지 않으면:
```
$ ./tetris --host 7777
[게임 종료]
$ ./tetris --host 7777
ERROR: Address already in use
```

`SO_REUSEADDR` 설정 시 즉시 재시작 가능.

**논블로킹 I/O 설정:**

```cpp
void set_nonblocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}
```

**왜 논블로킹인가?**

블로킹 소켓:
```cpp
recv(fd, buf, len, 0);  // 데이터 올 때까지 무한 대기
// → UI 프레임 드롭, 입력 불가
```

논블로킹 소켓:
```cpp
int n = recv(fd, buf, len, 0);
if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    // 데이터 없음, 다음 프레임에 재시도
    return;
}
```

**tcp_recv_some 구현:**

```cpp
bool tcp_recv_some(const TcpSocket& sock, std::vector<uint8_t>& buf) {
    uint8_t temp[4096];
    int n = recv(sock.fd, (char*)temp, sizeof(temp), 0);

    if (n > 0) {
        buf.insert(buf.end(), temp, temp + n);  // 버퍼에 추가
        return true;
    } else if (n == 0) {
        return false;  // 연결 종료
    } else {
        // EAGAIN/EWOULDBLOCK은 정상 (데이터 없음)
        if (would_block()) return true;
        return false;  // 실제 에러
    }
}
```

### 4.2 Framing Layer (net/framing.*)

TCP는 **스트림 프로토콜**이므로 메시지 경계를 명시해야 한다.

**문제 상황:**

```
송신: [HELLO][SEED][INPUT]
수신: [HEL][LO][SEED][IN][PUT]  // 패킷이 쪼개짐
```

**해결: 길이 기반 프레이밍**

```
┌────────┬────────┬─────────────┬──────────┐
│ LEN    │ TYPE   │ PAYLOAD     │ CHECKSUM │
│ 2bytes │ 1byte  │ LEN-1 bytes │ 4bytes   │
└────────┴────────┴─────────────┴──────────┘
```

**메시지 타입:**

```cpp
// net/framing.h
enum class MsgType : uint8_t {
    HELLO = 1,           // 프로토콜 버전 교환
    HELLO_ACK = 2,       // HELLO 응답
    SEED = 3,            // 게임 파라미터 (seed, start_tick, input_delay)
    INPUT = 4,           // 틱별 입력 비트마스크
    ACK = 5,             // 수신 확인
    PING = 6,            // Keepalive
    PONG = 7,            // Keepalive 응답
    HASH = 8,            // 상태 해시 (desync 감지)
    GAME_OVER_CHOICE = 9 // 게임 오버 후 선택
};
```

**프레임 구조체:**

```cpp
struct Frame {
    MsgType type;
    std::vector<uint8_t> payload;
};
```

**직렬화 (build_frame):**

```cpp
std::vector<uint8_t> build_frame(MsgType type, const std::vector<uint8_t>& payload) {
    uint16_t len = static_cast<uint16_t>(1 + payload.size());

    std::vector<uint8_t> frame;
    frame.reserve(2 + len + 4);

    // LEN (리틀 엔디안)
    frame.push_back(len & 0xFF);
    frame.push_back((len >> 8) & 0xFF);

    // TYPE
    frame.push_back(static_cast<uint8_t>(type));

    // PAYLOAD
    frame.insert(frame.end(), payload.begin(), payload.end());

    // CHECKSUM (FNV-1a)
    uint32_t checksum = fnv1a_32(frame.data(), frame.size());
    frame.push_back(checksum & 0xFF);
    frame.push_back((checksum >> 8) & 0xFF);
    frame.push_back((checksum >> 16) & 0xFF);
    frame.push_back((checksum >> 24) & 0xFF);

    return frame;
}
```

**역직렬화 (parse_frames):**

```cpp
void parse_frames(std::vector<uint8_t>& buf, std::vector<Frame>& out) {
    while (buf.size() >= 7) {  // 최소 크기: LEN(2) + TYPE(1) + CHECKSUM(4)
        // LEN 읽기
        uint16_t len = buf[0] | (buf[1] << 8);
        size_t total = 2 + len + 4;

        if (buf.size() < total) break;  // 불완전한 프레임

        // CHECKSUM 검증
        uint32_t expected = /* buf 마지막 4바이트 */;
        uint32_t actual = fnv1a_32(buf.data(), 2 + len);

        if (expected != actual) {
            std::cerr << "Checksum mismatch!" << std::endl;
            buf.erase(buf.begin(), buf.begin() + total);
            continue;
        }

        // TYPE + PAYLOAD 추출
        MsgType type = static_cast<MsgType>(buf[2]);
        std::vector<uint8_t> payload(buf.begin() + 3, buf.begin() + 2 + len);

        out.push_back(Frame{type, payload});
        buf.erase(buf.begin(), buf.begin() + total);
    }
}
```

**왜 체크섬이 필요한가?**

TCP는 이미 오류 검출을 하지만:
- 구현 버그 감지 (직렬화 오류)
- 메모리 손상 감지
- 디버깅 편의성

**리틀 엔디안 유틸리티:**

```cpp
// 쓰기
inline void le_write_u16(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(val & 0xFF);
    v.push_back((val >> 8) & 0xFF);
}

inline void le_write_u64(std::vector<uint8_t>& v, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        v.push_back((val >> (i * 8)) & 0xFF);
    }
}

// 읽기
inline uint64_t le_read_u64(const uint8_t* p) {
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (uint64_t)p[i] << (i * 8);
    }
    return val;
}
```

### 4.3 Session Layer (net/session.*)

P2P Lockstep 프로토콜의 핵심 구현.

**역할:**

1. 핸드셰이크 (HELLO ↔ SEED)
2. 입력 교환 (INPUT 메시지)
3. Lockstep 동기화 (안전 틱 계산)
4. 스레드 관리 (I/O, Accept)

**클래스 구조:**

```cpp
class Session {
public:
    // 연결
    bool Host(uint16_t port, const SeedParams& sp);
    bool Connect(const std::string& host, uint16_t port);

    // 상태
    bool isConnected() const;
    bool isReady() const;
    bool isListening() const;
    bool hasFailed() const;

    // 송신
    void SendInput(uint32_t tick, uint8_t mask);
    void SendGameOverChoice(GameOverChoice choice);
    void SendNewSeed(uint64_t newSeed);

    // 수신
    bool GetRemoteInput(uint32_t tick, uint8_t& outMask);
    bool GetRemoteGameOverChoice(GameOverChoice& outChoice) const;

    // 유틸
    uint32_t maxRemoteTick() const;
    void ClearInputs();
    void Close();

private:
    void ioThread();
    void acceptThread(uint16_t port);
    void handleFrame(const Frame& f);

    // 소켓
    TcpSocket sock{};
    TcpSocket listenSock{};

    // 스레드
    std::thread th;       // I/O 스레드
    std::thread ath;      // Accept 스레드 (Host만)

    // 상태 (atomic)
    std::atomic<bool> quit{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> ready{false};
    std::atomic<bool> listening{false};
    std::atomic<bool> connectionFailed{false};

    // 입력 큐 (mutex 보호)
    std::mutex inMu;
    std::unordered_map<uint32_t, uint8_t> remoteInputs;
    std::atomic<uint32_t> lastRemoteTick{0};
    std::atomic<uint32_t> lastLocalTick{0};

    // 송신 큐
    std::mutex sendMu;
    std::deque<std::vector<uint8_t>> sendQ;
};
```

**핸드셰이크 (Host):**

```cpp
bool Session::Host(uint16_t port, const SeedParams& sp) {
    // 상태 초기화 (재사용 대비)
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    remoteInputs.clear();
    lastRemoteTick = 0;
    lastLocalTick = 0;

    seedParams = sp;
    listening = true;

    // Accept 스레드 시작
    ath = std::thread(&Session::acceptThread, this, port);
    return true;
}

void Session::acceptThread(uint16_t port) {
    // 1. 포트 리스닝
    listenSock = tcp_listen(port, 1);
    if (!listenSock.valid()) {
        listening = false;
        return;
    }

    // 2. 클라이언트 대기 (블로킹)
    auto client = tcp_accept(listenSock);
    tcp_close(listenSock);

    if (!client.valid()) {
        listening = false;
        return;
    }

    // 3. 연결 완료
    sock = client;
    connected = true;
    listening = false;

    // 4. I/O 스레드 시작
    th = std::thread(&Session::ioThread, this);

    // 5. HELLO 전송
    {
        std::vector<uint8_t> pl;
        le_write_u16(pl, 1);  // 프로토콜 버전
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu);
        sendQ.push_back(std::move(fr));
    }

    // 6. SEED 전송
    {
        std::vector<uint8_t> pl;
        le_write_u64(pl, seedParams.seed);
        le_write_u32(pl, seedParams.start_tick);
        pl.push_back(seedParams.input_delay);
        pl.push_back((uint8_t)seedParams.role);
        auto fr = build_frame(MsgType::SEED, pl);
        std::lock_guard<std::mutex> lk(sendMu);
        sendQ.push_back(std::move(fr));
    }

    // 7. Ready 플래그 설정 (중요!)
    ready = true;
}
```

**핸드셰이크 (Client):**

```cpp
bool Session::Connect(const std::string& host, uint16_t port) {
    // 상태 초기화
    quit = false;
    connectionFailed = false;
    connected = false;
    ready = false;
    listening = false;
    remoteInputs.clear();

    // 연결 시도
    sock = tcp_connect(host, port);
    if (!sock.valid()) {
        connectionFailed = true;
        return false;
    }

    connected = true;

    // I/O 스레드 시작
    th = std::thread(&Session::ioThread, this);

    // HELLO 전송
    {
        std::vector<uint8_t> pl;
        le_write_u16(pl, 1);
        auto fr = build_frame(MsgType::HELLO, pl);
        std::lock_guard<std::mutex> lk(sendMu);
        sendQ.push_back(std::move(fr));
    }

    return true;
}
```

**I/O 스레드:**

```cpp
void Session::ioThread() {
    auto startTime = std::chrono::steady_clock::now();
    const auto CONNECTION_TIMEOUT = std::chrono::seconds(10);

    while (!quit.load()) {
        // 타임아웃 체크 (ready 전까지만)
        if (!ready.load()) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > CONNECTION_TIMEOUT) {
                connectionFailed = true;
                quit = true;
                break;
            }
        }

        // 수신
        size_t prevSize = recvBuf.size();
        if (tcp_recv_some(sock, recvBuf)) {
            if (recvBuf.size() > prevSize) {
                std::vector<Frame> frames;
                parse_frames(recvBuf, frames);
                for (auto& f : frames) {
                    handleFrame(f);
                }
            }
        } else {
            // 연결 끊김
            connectionFailed = true;
            quit = true;
            break;
        }

        // 송신
        {
            std::lock_guard<std::mutex> lk(sendMu);
            while (!sendQ.empty()) {
                auto& pkt = sendQ.front();
                if (!tcp_send_all(sock, pkt.data(), pkt.size())) {
                    quit = true;
                    break;
                }
                sendQ.pop_front();
            }
        }

        // CPU 절약
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
```

**메시지 처리:**

```cpp
void Session::handleFrame(const Frame& f) {
    switch (f.type) {
    case MsgType::HELLO:
        // HELLO_ACK 응답
        {
            std::vector<uint8_t> pl;
            pl.push_back(1);
            auto fr = build_frame(MsgType::HELLO_ACK, pl);
            std::lock_guard<std::mutex> lk(sendMu);
            sendQ.push_back(std::move(fr));
        }
        break;

    case MsgType::SEED:
        // 클라이언트: SEED 파라미터 수신
        if (f.payload.size() >= 14) {
            const uint8_t* p = f.payload.data();
            seedParams.seed = le_read_u64(p);
            seedParams.start_tick = le_read_u32(p + 8);
            seedParams.input_delay = p[12];
            seedParams.role = (Role)p[13];
            ready = true;  // 클라이언트 ready
        }
        break;

    case MsgType::INPUT:
        // 입력 수신
        if (f.payload.size() >= 6) {
            const uint8_t* p = f.payload.data();
            uint32_t from = le_read_u32(p);
            uint16_t cnt = le_read_u16(p + 4);
            const uint8_t* arr = p + 6;

            std::lock_guard<std::mutex> lk(inMu);
            for (uint16_t i = 0; i < cnt; ++i) {
                uint32_t tick = from + i;
                remoteInputs[tick] = arr[i];
                if (tick > lastRemoteTick) {
                    lastRemoteTick = tick;
                }
            }
        }
        break;
    }
}
```

**Lockstep 안전 틱 계산 (main.cpp):**

```cpp
// 양쪽이 보낸 입력 중 최소값
int64_t lastLocalSent = (localTickNext == 0) ? -1 : (int64_t)localTickNext - 1;
int64_t lastRemote = (int64_t)session.maxRemoteTick();

// 입력 지연을 빼서 안전 틱 계산
int64_t safeTickInclusive = std::min(lastLocalSent, lastRemote) - (int64_t)inputDelay;

// 안전 틱까지만 시뮬레이션
while ((int64_t)simTick <= safeTickInclusive) {
    uint8_t localInput = localInputs[simTick];
    uint8_t remoteInput;
    if (!session.GetRemoteInput(simTick, remoteInput)) break;

    gameLocal->SubmitInput(localInput);
    gameRemote->SubmitInput(remoteInput);
    gameLocal->Tick();
    gameRemote->Tick();
    simTick++;
}
```

**왜 inputDelay가 필요한가?**

```
inputDelay = 0:
  Tick 10에서 입력 → 즉시 Tick 10 시뮬레이션
  문제: 네트워크 지연 시 대기 → 버벅임

inputDelay = 2:
  Tick 10에서 입력 → Tick 12에 적용
  여유: 2틱 안에 도착하면 부드러움
```

---

## 5. 게임 로직과 상태 관리

### 5.1 Game 클래스 (src/game.h)

```cpp
class Game {
public:
    explicit Game(uint64_t seed);

    void Tick();
    void SubmitInput(uint8_t mask);
    void Draw();
    void DrawBoardAt(int x, int y);

    uint64_t ComputeStateHash() const;

    bool gameOver{false};
    int score{0};
    Music music;

private:
    void SpawnNewBlock();
    void MoveLeft();
    void MoveRight();
    void MoveDown();
    void Rotate();
    void HardDrop();
    void LockBlock();
    void ClearLines();

    Grid grid;
    Block currentBlock;
    Block nextBlock;
    XorShift64Star rng;
    int gravityCounter{0};
};
```

**초기화:**

```cpp
Game::Game(uint64_t seed) : rng(seed) {
    grid.Clear();
    SpawnNewBlock();
    music = LoadMusicStream("Sounds/music.mp3");
    PlayMusicStream(music);
}
```

**틱 진행:**

```cpp
void Game::Tick() {
    if (gameOver) return;

    // 중력 (자동 낙하)
    gravityCounter++;
    if (gravityCounter >= 60) {  // 1초마다
        MoveDown();
        gravityCounter = 0;
    }
}
```

**입력 적용:**

```cpp
void Game::SubmitInput(uint8_t mask) {
    if (gameOver) return;

    if (mask & INPUT_LEFT)   MoveLeft();
    if (mask & INPUT_RIGHT)  MoveRight();
    if (mask & INPUT_DOWN)   MoveDown();
    if (mask & INPUT_ROTATE) Rotate();
    if (mask & INPUT_DROP)   HardDrop();
}
```

**블록 이동 (예시: MoveLeft):**

```cpp
void Game::MoveLeft() {
    currentBlock.x--;
    if (grid.CheckCollision(currentBlock)) {
        currentBlock.x++;  // 충돌 시 되돌림
    }
}
```

**회전 (SRS - Super Rotation System):**

```cpp
void Game::Rotate() {
    Block rotated = currentBlock;
    rotated.rotation = (rotated.rotation + 1) % 4;

    if (!grid.CheckCollision(rotated)) {
        currentBlock = rotated;
    } else {
        // Wall Kick 시도
        for (auto [dx, dy] : wallKickOffsets) {
            rotated.x += dx;
            rotated.y += dy;
            if (!grid.CheckCollision(rotated)) {
                currentBlock = rotated;
                return;
            }
            rotated.x -= dx;
            rotated.y -= dy;
        }
    }
}
```

**라인 클리어:**

```cpp
void Game::ClearLines() {
    int linesCleared = 0;

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        bool full = true;
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (grid.cells[y][x] == 0) {
                full = false;
                break;
            }
        }

        if (full) {
            // 라인 제거 및 위에서 아래로 이동
            for (int yy = y; yy > 0; --yy) {
                grid.cells[yy] = grid.cells[yy - 1];
            }
            grid.cells[0].fill(0);
            linesCleared++;
        }
    }

    // 점수 계산
    static const int points[] = {0, 100, 300, 500, 800};
    score += points[linesCleared];
}
```

### 5.2 상태 머신 (main.cpp)

**애플리케이션 모드:**

```cpp
enum class AppMode {
    Menu,           // 메인 메뉴
    ConnectInput,   // IP:port 입력
    Single,         // 싱글플레이
    Net             // 네트워크 플레이
};
```

**게임 오버 상태:**

```cpp
enum class GameOverState {
    None,                // 게임 중
    ShowingGameOver,     // 게임 오버 표시, 선택 대기
    WaitingForRemote,    // 내 선택 완료, 상대 대기
    ShowingDisagreement, // 선택 불일치 (3초 카운트다운)
    SendingNewSeed,      // 호스트: 새 시드 전송 중
    WaitingForNewSeed,   // 클라이언트: 새 시드 대기
    RestartingGame,      // 재시작 준비
    GoingToTitle         // 타이틀 복귀
};
```

**상태 전이 흐름 (Restart 선택 시):**

```
양쪽이 R 키 선택
    ↓
ShowingGameOver → WaitingForRemote
    ↓
양쪽 선택 수신 및 비교
    ↓
Host: SendingNewSeed (새 시드 생성 및 전송)
Client: WaitingForNewSeed (SEED 메시지 대기)
    ↓
1.5초 후 (Host) / SEED 수신 (Client)
    ↓
양쪽: RestartingGame
    ↓
입력 큐 초기화
게임 재생성
    ↓
None (게임 시작)
```

**재시작 구현 (Host):**

```cpp
else if (gameOverState == GameOverState::SendingNewSeed) {
    DrawTextEx(font, "Sending new seed...", {180, 450}, 24, 2, GRAY);

    // 새 시드 생성 (최초 1회)
    if (!newSeedSent) {
        sessionSeed = ((uint64_t)GetTime() * 1000000.0) + rand();
        session.SendNewSeed(sessionSeed);
        std::cout << "[GAME] Host generating new seed: 0x"
                  << std::hex << sessionSeed << std::dec << std::endl;
        newSeedSent = true;
    }

    // 1.5초 대기 (Client가 받을 시간)
    gameOverTimer += deltaTime;
    if (gameOverTimer >= 1.5f) {
        newSeedSent = false;
        gameOverState = GameOverState::RestartingGame;
    }
}
```

**재시작 구현 (Client):**

```cpp
else if (gameOverState == GameOverState::WaitingForNewSeed) {
    DrawTextEx(font, "Waiting for new seed...", {180, 450}, 24, 2, GRAY);

    // SEED 메시지 수신 확인
    uint64_t newSeed = session.params().seed;

    if (newSeed != lastReceivedSeed) {
        std::cout << "[GAME] Client received new seed: 0x"
                  << std::hex << newSeed << std::dec << std::endl;
        lastReceivedSeed = newSeed;
        gameOverState = GameOverState::RestartingGame;
    }

    // 타임아웃 (10초)
    gameOverTimer += deltaTime;
    if (gameOverTimer >= 10.0f) {
        gameOverState = GameOverState::GoingToTitle;
    }
}
```

**공통 재시작 로직:**

```cpp
else if (gameOverState == GameOverState::RestartingGame) {
    // 양쪽 모두 session.params().seed 사용 (동기화 보장)
    sessionSeed = session.params().seed;
    std::cout << "[GAME] Restarting with seed: 0x"
              << std::hex << sessionSeed << std::dec << std::endl;

    // 입력 큐 초기화 (중요!)
    session.ClearInputs();
    localInputs.clear();
    localTickNext = 0;
    simTick = 0;
    startDelay = session.params().start_tick;

    // 게임 재생성
    gameLocal = std::make_unique<Game>(sessionSeed);
    gameRemote = std::make_unique<Game>(sessionSeed);

    session.ClearGameOverChoices();
    gameOverState = GameOverState::None;
}
```

---

## 6. 멀티플레이어 통합 과정

### 6.1 싱글플레이 → 멀티플레이 전환

**초기 싱글플레이 코드:**

```cpp
// 단순한 루프
while (accumulator >= SECONDS_PER_TICK) {
    uint8_t input = SampleInput();
    game.SubmitInput(input);
    game.Tick();
    accumulator -= SECONDS_PER_TICK;
}

game.Draw();
```

**문제점:**

- 입력이 즉시 적용됨 (네트워크 지연 고려 없음)
- 단일 게임 상태만 존재
- 상대방 입력을 받을 방법 없음

**멀티플레이 변환:**

```cpp
// 1. 두 개의 게임 인스턴스
std::unique_ptr<Game> gameLocal;   // 내 게임
std::unique_ptr<Game> gameRemote;  // 상대 게임

// 2. 입력 버퍼링
std::unordered_map<uint32_t, uint8_t> localInputs;
uint32_t localTickNext = 0;
uint32_t simTick = 0;

// 3. 입력 수집 및 전송 (매 프레임)
while (accumulator >= SECONDS_PER_TICK) {
    uint8_t input = SampleInput();
    localInputs[localTickNext] = input;
    session.SendInput(localTickNext, input);
    localTickNext++;

    // 시뮬레이션은 나중에 (안전 틱 확인 후)
    accumulator -= SECONDS_PER_TICK;
}

// 4. 안전 틱까지만 시뮬레이션
if (session.isReady()) {
    int64_t safeTick = CalculateSafeTick();
    while (simTick <= safeTick) {
        uint8_t li = localInputs[simTick];
        uint8_t ri;
        if (!session.GetRemoteInput(simTick, ri)) break;

        gameLocal->SubmitInput(li);
        gameRemote->SubmitInput(ri);
        gameLocal->Tick();
        gameRemote->Tick();
        simTick++;
    }
}

// 5. 듀얼 보드 렌더링
gameLocal->DrawBoardAt(11, 11);
gameRemote->DrawBoardAt(371, 11);
```

### 6.2 핸드셰이크 흐름

**CLI 인자 처리:**

```cpp
// main.cpp:62-68
bool netMode = false;
bool isHost = false;
std::string hostIp;
uint16_t hostPort = 7777;

for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host") {
        isHost = true;
        netMode = true;
        if (i + 1 < argc) {
            hostPort = std::stoi(argv[i + 1]);
            i++;
        }
    } else if (arg == "--connect") {
        netMode = true;
        if (i + 1 < argc) {
            std::string endpoint = argv[i + 1];
            auto pos = endpoint.find(":");
            hostIp = endpoint.substr(0, pos);
            hostPort = std::stoi(endpoint.substr(pos + 1));
            i++;
        }
    }
}
```

**CLI 모드 시작:**

```cpp
if (app == AppMode::Net) {
    if (isHost) {
        sessionSeed = ((uint64_t)GetTime() * 1000000.0) + 0xC0FFEEULL;
        net::SeedParams sp{sessionSeed, startDelay, inputDelay, net::Role::Host};
        if (!session.Host(hostPort, sp)) {
            TraceLog(LOG_ERROR, "Host failed");
            return 1;
        }
    } else {
        if (!session.Connect(hostIp, hostPort)) {
            TraceLog(LOG_ERROR, "Connect failed");
            return 1;
        }
    }
}
```

**보드 초기화 시점:**

```cpp
// main.cpp:155-163
if (session.isReady() && (!gameLocal || !gameRemote)) {
    // SEED 메시지 수신 완료 → 보드 생성
    sessionSeed = session.params().seed;
    inputDelay = session.params().input_delay;

    gameLocal = std::make_unique<Game>(sessionSeed);
    gameRemote = std::make_unique<Game>(sessionSeed);

    localInputs.clear();
    localTickNext = 0;
    simTick = 0;
    startDelay = session.params().start_tick;
}
```

### 6.3 듀얼 보드 렌더링

```cpp
// main.cpp:350-363
if (app == AppMode::Net && gameLocal && gameRemote) {
    ClearBackground(darkBlue);

    Vector2 leftOrigin{11, 11};
    Vector2 rightOrigin{11 + 300 + 60, 11};

    DrawTextEx(font, "Local", {leftOrigin.x, 8}, 22, 2, WHITE);
    DrawTextEx(font, "Remote", {rightOrigin.x, 8}, 22, 2, WHITE);

    gameLocal->DrawBoardAt((int)leftOrigin.x, (int)leftOrigin.y);
    gameRemote->DrawBoardAt((int)rightOrigin.x, (int)rightOrigin.y);

    DrawTextEx(font, TextFormat("Score: %d", gameLocal->score),
               {leftOrigin.x, 620 - 28}, 20, 1, WHITE);
    DrawTextEx(font, TextFormat("Score: %d", gameRemote->score),
               {rightOrigin.x, 620 - 28}, 20, 1, WHITE);
}
```

---

## 7. 직면한 문제와 해결책

이 섹션에서는 개발 중 마주쳤던 실제 문제들과 해결 과정을 다룬다.

### 7.1 문제: ESC 키로 창이 닫힘

**증상:**

```cpp
// 게임 오버 화면
DrawTextEx(font, "[ESC] Go to Title", ...);

// 사용자가 ESC 누름 → Raylib 창이 닫힘
```

**원인:**

Raylib의 `WindowShouldClose()`는 기본적으로 ESC 키도 감지한다:

```cpp
while (WindowShouldClose() == false) {
    // ESC 누르면 바로 루프 종료
}
```

**해결:**

ESC 대신 Q 키 사용:

```cpp
// 모든 ESC 참조를 Q로 변경
DrawTextEx(font, "[Q] Go to Title", ...);

if (IsKeyPressed(KEY_Q)) {
    app = AppMode::Menu;
}
```

**학습 포인트:**

라이브러리의 기본 동작을 항상 확인해야 한다. Raylib 문서를 읽으면:

```c
// raylib.h
bool WindowShouldClose(void);  // Detect window close button or ESC key
```

명시되어 있었다.

### 7.2 문제: Restart 후 블록이 다름 (Desync)

**증상:**

```
Host: O 블록 생성
Client: L 블록 생성
→ 이후 모든 입력이 엇갈림
```

**디버깅 과정:**

1. 해시 확인:
```cpp
if (IsKeyPressed(KEY_H)) {
    std::cout << "Local:  0x" << std::hex << gameLocal->ComputeStateHash() << std::endl;
    std::cout << "Remote: 0x" << std::hex << gameRemote->ComputeStateHash() << std::endl;
}

// 출력:
// Local:  0x12AB34CD
// Remote: 0x56EF78AB  ← 불일치!
```

2. 시드 로그 추가:
```cpp
std::cout << "[GAME] Host sending new seed: 0x" << std::hex << sessionSeed << std::endl;
std::cout << "[GAME] Client received seed: 0x" << std::hex << newSeed << std::endl;
std::cout << "[GAME] Restarting with seed: 0x" << std::hex << sessionSeed << std::endl;

// 출력:
// [GAME] Host sending new seed: 0xABCD1234
// [GAME] Client received seed: 0xABCD1234
// [GAME] Restarting with seed: 0xEF567890  ← Host가 다른 시드 사용!
```

**원인:**

Host가 두 개의 다른 시드를 사용하고 있었다:

```cpp
// SendingNewSeed 상태
sessionSeed = GenerateNewSeed();  // 0xABCD1234
session.SendNewSeed(sessionSeed); // 클라이언트에 전송

// RestartingGame 상태
Game(sessionSeed);  // 그런데 sessionSeed가 변경됨!
```

**해결:**

양쪽 모두 `session.params().seed`를 읽도록 통일:

```cpp
// main.cpp:500
else if (gameOverState == GameOverState::RestartingGame) {
    // 중요: 양쪽 모두 session.params().seed 사용
    sessionSeed = session.params().seed;  // ← 추가

    gameLocal = std::make_unique<Game>(sessionSeed);
    gameRemote = std::make_unique<Game>(sessionSeed);
}
```

**학습 포인트:**

- 멀티플레이어에서는 **단일 진실 원천(Single Source of Truth)** 필요
- 시드는 `session.params()`에 저장되고, 모든 곳에서 이를 읽어야 함
- 로그 출력으로 실제 사용되는 값을 추적해야 함

### 7.3 문제: Restart 후 이전 입력이 재생됨

**증상:**

```
게임 1: 플레이어가 왼쪽 이동
게임 1 종료
재시작
게임 2: 블록이 자동으로 왼쪽 이동 (입력 안 했는데!)
```

**원인:**

`remoteInputs` 맵이 초기화되지 않음:

```cpp
// session.h
std::unordered_map<uint32_t, uint8_t> remoteInputs;

// 게임 1:
remoteInputs[0] = INPUT_LEFT;
remoteInputs[1] = INPUT_RIGHT;
// ...

// 재시작 시 그대로 남아있음!
```

**해결:**

Session에 입력 큐 초기화 메서드 추가:

```cpp
// net/session.h
void ClearInputs();

// net/session.cpp
void Session::ClearInputs() {
    std::lock_guard<std::mutex> lk(inMu);
    remoteInputs.clear();
    lastRemoteTick.store(0);
    lastLocalTick.store(0);
    std::cout << "[NET] Cleared input queues" << std::endl;
}

// main.cpp - RestartingGame 상태
session.ClearInputs();  // ← 추가
localInputs.clear();
localTickNext = 0;
simTick = 0;
```

**학습 포인트:**

- 상태 초기화는 명시적으로 해야 함 (자동으로 되지 않음)
- 재시작/재연결 시나리오를 테스트해야 함
- 멀티스레드 환경에서는 mutex 보호 필수

### 7.4 문제: Host 재시작 후 연결 실패

**증상:**

```
1. 호스트 시작 → 클라이언트 연결 → 게임
2. Q 키로 타이틀 복귀
3. 다시 Host 선택
4. Host가 멈춤 (listening 상태에서 진행 안 됨)
5. Client 연결 시도 → "Connection Failed"
```

**디버깅:**

```cpp
// acceptThread 로그 추가
void Session::acceptThread(uint16_t port) {
    std::cout << "[NET] Accept thread started" << std::endl;

    auto client = tcp_accept(listenSock);
    std::cout << "[NET] Client accepted" << std::endl;

    // ... HELLO, SEED 전송 ...

    std::cout << "[NET] Ready flag set" << std::endl;  // ← 출력 안됨!
}
```

**원인:**

`Session::Close()`가 `quit = true`로 설정하는데, `Host()` 재호출 시 리셋하지 않음:

```cpp
void Session::Close() {
    quit = true;  // ← 이 플래그가 남아있음
    // ...
}

// ioThread:
while (!quit.load()) {  // ← 바로 종료
    // ...
}
```

**해결:**

`Host()` 및 `Connect()`에서 모든 상태 리셋:

```cpp
bool Session::Host(uint16_t port, const SeedParams& sp) {
    // Close() 이후 재사용을 위한 상태 리셋
    quit = false;                // ← 추가
    connectionFailed = false;
    connected = false;
    ready = false;
    remoteInputs.clear();
    lastRemoteTick = 0;
    lastLocalTick = 0;
    recvBuf.clear();

    seedParams = sp;
    listening = true;
    ath = std::thread(&Session::acceptThread, this, port);
    return true;
}
```

**학습 포인트:**

- 객체 재사용 시 모든 상태를 명시적으로 초기화해야 함
- 생성자는 한 번만 호출되므로, 재시작 로직은 별도 필요
- 플래그 기반 제어는 모든 경로에서 일관성 유지 필수

### 7.5 문제: Host의 ready 플래그 누락

**증상:**

```
Host: 클라이언트 accept 성공
Host: HELLO, SEED 전송
Host: 10초 후 "Connection timeout"
Client: 10초 후 "Connection timeout"
```

**로그 분석:**

```
[NET] Client connected!
[NET] Queued HELLO message
[NET] Queued SEED message (seed=0x12345678)
[NET] I/O thread started
[NET] Received 11 bytes
[NET] Parsed 1 frames
[NET] Connection timeout after 10 seconds
```

받은 메시지는 있는데 타임아웃? → `ready` 플래그 확인 필요

**코드 확인:**

```cpp
// ioThread:
if (!ready.load()) {
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    if (elapsed > CONNECTION_TIMEOUT) {
        std::cout << "[NET] Connection timeout" << std::endl;
        connectionFailed = true;
        quit = true;
        break;
    }
}
```

`ready`가 false면 계속 타임아웃 체크 → `acceptThread`에서 `ready = true` 설정을 확인:

```cpp
void Session::acceptThread(uint16_t port) {
    // ...
    // HELLO, SEED 전송
    // ready = true;  ← 이 줄이 없었음!
    std::cout << "[NET] Host session waiting..." << std::endl;
}
```

**해결:**

```cpp
void Session::acceptThread(uint16_t port) {
    // ... HELLO, SEED 전송 ...

    ready = true;  // ← 추가
    std::cout << "[NET] Host session is ready!" << std::endl;
}
```

**학습 포인트:**

- 상태 플래그는 모든 경로에서 올바르게 설정되어야 함
- Client는 `handleFrame(SEED)`에서 `ready = true` 설정
- Host는 `acceptThread`에서 `ready = true` 설정
- 비대칭 로직은 양쪽을 모두 확인해야 함

### 7.6 문제: 창 이동 시 시드 동기화 실패

**증상:**

```
1. 게임 오버
2. 양쪽 R 키 선택
3. Host의 "Sending new seed..." 화면에서 창 이동
4. 창이 멈춤 (Windows 메시지 루프 블로킹)
5. 창 이동 끝
6. Host와 Client가 다른 블록 생성
```

**원인:**

```cpp
// 창 이동 중:
GetFrameTime() → 5.0f (5초 경과)

// 타이머 업데이트:
gameOverTimer += GetFrameTime();  // += 5.0
if (gameOverTimer >= 1.5f) {      // 즉시 통과!
    gameOverState = RestartingGame;
}
```

Host가 1.5초를 기다리지 않고 바로 진행 → Client는 아직 SEED 못 받음 → 다른 시드 사용

**해결: Delta Time Clamping**

```cpp
// main.cpp:136-141
float deltaTime = GetFrameTime();
const float MAX_DELTA = 0.1f;  // 최대 100ms (10 FPS)

if (deltaTime > MAX_DELTA) {
    std::cout << "[WARNING] Large delta time: " << deltaTime
              << "s, clamping to " << MAX_DELTA << "s" << std::endl;
    deltaTime = MAX_DELTA;
}

// 모든 타이머에 clamped delta 사용
gameOverTimer += deltaTime;  // GetFrameTime() 대신
```

**학습 포인트:**

- 윈도우 시스템에서는 창 이동/리사이즈 시 메인 루프가 블로킹됨
- 큰 delta time은 게임 로직을 망가뜨릴 수 있음
- Clamping은 일반적인 게임 개발 베스트 프랙티스
- Unity, Unreal 등도 비슷한 문제를 백그라운드 스레드로 해결

### 7.7 문제: Host와 Client의 타이밍 불일치

**증상:**

```
Host: RestartingGame 즉시 전환 → 게임 생성 → startDelay 카운트다운 시작
Client: WaitingForNewSeed → (네트워크 지연) → RestartingGame → 게임 생성
결과: Host가 먼저 시뮬레이션 시작 → 입력 틱 어긋남
```

**해결: SendingNewSeed 상태 추가**

```cpp
enum class GameOverState {
    // ...
    SendingNewSeed,      // Host: 새 시드 전송 후 대기
    WaitingForNewSeed,   // Client: 새 시드 대기
    RestartingGame,
    // ...
};

// Host 흐름:
ShowingGameOver → WaitingForRemote → SendingNewSeed (1.5초 대기) → RestartingGame

// Client 흐름:
ShowingGameOver → WaitingForRemote → WaitingForNewSeed → RestartingGame
```

**구현:**

```cpp
if (session.params().role == net::Role::Host) {
    gameOverState = GameOverState::SendingNewSeed;
} else {
    gameOverState = GameOverState::WaitingForNewSeed;
}

// SendingNewSeed 처리:
if (!newSeedSent) {
    sessionSeed = GenerateNewSeed();
    session.SendNewSeed(sessionSeed);
    newSeedSent = true;
}

gameOverTimer += deltaTime;
if (gameOverTimer >= 1.5f) {  // 1.5초 대기
    gameOverState = GameOverState::RestartingGame;
}
```

**학습 포인트:**

- P2P에서는 양쪽의 타이밍 동기화가 중요
- 네트워크 지연을 고려한 대기 시간 필요
- 상태 머신을 세분화하여 명확한 제어 가능

---

## 8. 성능 최적화와 디버깅

### 8.1 네트워크 대역폭 분석

**현재 사용량:**

```
입력 메시지 크기:
  LEN(2) + TYPE(1) + [TICK(4) + COUNT(2) + INPUT(1)] + CHECKSUM(4)
  = 14 바이트 / 메시지

전송 빈도: 60 Hz (매 틱)

대역폭: 14 bytes × 60 Hz = 840 bytes/sec ≈ 6.7 Kbps (양방향)
```

**최적화 가능성:**

1. **배치 전송** (20Hz로 감소):
```cpp
// 현재: 매 틱 전송
session.SendInput(tick, input);

// 최적화: 3틱마다 전송
if (localTickNext % 3 == 0) {
    std::vector<uint8_t> batch;
    for (int i = 0; i < 3; ++i) {
        batch.push_back(localInputs[localTickNext - 2 + i]);
    }
    session.SendInputBatch(localTickNext - 2, batch);
}

// 대역폭: 840 / 3 = 280 bytes/sec
```

2. **런-렝스 인코딩**:
```cpp
// 같은 입력 반복 시 압축
// 예: [0x02, 0x02, 0x02, 0x02, 0x02]
// →   [RLE: 0x02, count: 5]
```

### 8.2 디버깅 도구

**상태 해시 검증:**

```cpp
// main.cpp:321-329
if (IsKeyPressed(KEY_H)) {
    uint64_t h1 = gameSingle ? gameSingle->ComputeStateHash() : 0;
    uint64_t hL = gameLocal ? gameLocal->ComputeStateHash() : 0;
    uint64_t hR = gameRemote ? gameRemote->ComputeStateHash() : 0;

    std::cout << "Hash(single): 0x" << std::hex << h1
              << " local: 0x" << hL
              << " remote: 0x" << hR << std::dec << std::endl;
}
```

**네트워크 상태 HUD:**

```cpp
// main.cpp:575-580
if (app == AppMode::Net) {
    DrawText(TextFormat("NET: %s", session.isConnected() ? "CONNECTED" : "DISCONNECTED"),
             10, 580, 10, RAYWHITE);

    if (session.isReady()) {
        DrawText(TextFormat("SEED: 0x%08x", (unsigned)(sessionSeed & 0xFFFFFFFFu)),
                 10, 594, 10, RAYWHITE);
        DrawText(TextFormat("TICKS localSent=%u remoteMax=%u sim=%u delay=%u",
                           (unsigned)localTickNext - 1,
                           (unsigned)session.maxRemoteTick(),
                           (unsigned)simTick,
                           (unsigned)inputDelay),
                 10, 606, 10, RAYWHITE);
    }
}
```

**콘솔 로깅:**

```cpp
// 모든 주요 이벤트에 로그 추가
std::cout << "[NET] Connecting to " << host << ":" << port << std::endl;
std::cout << "[NET] Client connected!" << std::endl;
std::cout << "[NET] Received SEED: seed=0x" << std::hex << seed << std::dec << std::endl;
std::cout << "[GAME] Both chose Restart" << std::endl;
std::cout << "[GAME] Restarting with seed: 0x" << std::hex << sessionSeed << std::dec << std::endl;
```

### 8.3 프로파일링

**틱 시간 측정:**

```cpp
auto start = std::chrono::high_resolution_clock::now();

game.Tick();

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

if (duration.count() > 1000) {  // 1ms 초과 시
    std::cout << "[PERF] Tick took " << duration.count() << " us" << std::endl;
}
```

**병목 구간 식별:**

```
일반적인 틱: ~100us
라인 클리어 틱: ~500us (ClearLines 함수)
→ 최적화 필요 없음 (16ms 예산의 3%)
```

---

## 9. 배운 점과 개선 방향

### 9.1 기술적 학습

**네트워킹:**

- TCP 소켓 프로그래밍 (WinSock2, BSD Sockets)
- 플랫폼 독립적인 추상화 레이어 설계
- 메시지 프레이밍과 직렬화
- 논블로킹 I/O와 멀티스레딩
- 상태 동기화 vs 입력 동기화

**게임 아키텍처:**

- Fixed timestep 시뮬레이션
- 결정론적 시스템 설계
- 상태 머신 패턴
- 계층 분리와 관심사 분리

**C++ 고급 기법:**

- 스마트 포인터 (`std::unique_ptr`, `std::make_unique`)
- 원자적 연산 (`std::atomic`)
- 뮤텍스와 락 (`std::mutex`, `std::lock_guard`)
- 스레드 관리 (`std::thread`)
- 비트 연산 (입력 비트마스크)

### 9.2 개선 방향

**단기 개선:**

1. **State Pattern 적용**:
```cpp
class IGameState {
    virtual void Update(float deltaTime) = 0;
    virtual void Draw() = 0;
};

class MenuState : public IGameState { ... };
class PlayingState : public IGameState { ... };
class GameOverState : public IGameState { ... };
```

2. **재연결 로직**:
```cpp
// 일시적 연결 끊김 복구
if (session.hasFailed()) {
    if (auto saved = session.GetLastState()) {
        session.Reconnect(host, port);
        session.RestoreState(saved);
    }
}
```

3. **입력 배치**:
```cpp
// 60Hz → 20Hz로 감소
if (localTickNext % 3 == 0) {
    session.SendInputBatch(localTickNext - 2, lastThreeInputs);
}
```

**중기 개선:**

4. **ENet 마이그레이션**:
```cpp
// TCP → UDP + 신뢰성 채널
ENetHost* client = enet_host_create(NULL, 1, 2, 0, 0);
ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
```

5. **Client-Server 전환**:
```cpp
// P2P → Dedicated Server
class Server {
    std::vector<Client*> clients;
    void BroadcastState();
};
```

**장기 개선:**

6. **엔진 포팅**:
- Unity + Mirror/Netcode for GameObjects
- Godot + High-level Multiplayer API
- 비주얼 향상, 파티클, 애니메이션

7. **AI 통합**:
```cpp
// Python PyTorch 모델 연동
class TetrisBot {
    torch::jit::script::Module model;
    uint8_t DecideAction(const Game& state);
};
```

8. **DB 연동**:
```cpp
// SQLite로 통계 저장
class StatsDB {
    void SaveMatch(uint64_t seed, int score, const ReplayData& replay);
    std::vector<Leaderboard> GetTop10();
};
```

### 9.3 일반적인 게임 개발과의 차이

**현재 접근 (교육/프로토타입):**

- Raw socket 직접 구현
- 텍스트 기반 프레이밍
- main.cpp에 모든 로직
- 수동 상태 동기화

**상용 게임 접근:**

- 네트워킹 라이브러리 사용 (ENet, SteamNetworkingSockets)
- Protobuf/FlatBuffers 메시지
- 엔진 내장 네트워킹 (Unity, Unreal)
- 자동 Replication

**하지만 이 프로젝트의 가치:**

- 네트워킹 기초를 완전히 이해
- 디버깅 능력 향상
- 최적화 근거 파악
- 라이브러리 선택 시 trade-off 이해

### 9.4 최종 평가

**강점:**

- 결정론적 시뮬레이션 완벽 구현
- 플랫폼 독립적 설계
- 명확한 계층 분리
- 리플레이 시스템으로 검증 가능
- 외부 의존성 최소화

**약점:**

- 코드 유지보수성 (main.cpp 비대화)
- 에러 처리 부족 (타임아웃만 존재)
- 재연결 미지원
- 3인 이상 플레이 불가

**적합한 사용처:**

- 교육 목적: ⭐⭐⭐⭐⭐
- 포트폴리오: ⭐⭐⭐⭐⭐
- 친구와 플레이: ⭐⭐⭐⭐
- 상용 출시: ⭐⭐

---

## 결론

이 프로젝트는 C++로 멀티플레이어 게임의 핵심 개념을 처음부터 구현한 사례입니다. TCP 소켓, 메시지 프레이밍, Lockstep 동기화, 결정론적 시뮬레이션 등 모든 요소를 직접 구현하면서 네트워킹의 기초를 깊이 이해할 수 있었습니다.

상용 게임 개발에서는 라이브러리나 엔진을 사용하는 것이 효율적이지만, 이렇게 low-level부터 구현해보는 것은 다음과 같은 이점이 있습니다:

1. **문제 해결 능력**: 네트워크 문제 디버깅 시 근본 원인 파악 가능
2. **최적화 근거**: "왜 이 라이브러리를 쓰는가?"에 대한 명확한 답
3. **설계 판단**: 아키텍처 선택 시 trade-off 이해
4. **확장성**: 특수한 요구사항 발생 시 커스터마이징 가능

향후 이 코드베이스에 강화학습 AI, 데이터베이스 통계, 더 복잡한 게임 모드 등을 추가하면 종합적인 게임 프로젝트로 발전시킬 수 있을 것입니다.

---

**GitHub 저장소**: [여기에 링크 추가]

**주요 파일:**
- `DOCUMENTATION.md` - 전체 기술 문서
- `CLAUDE.md` - 프로젝트 가이드
- `src/main.cpp` - 메인 로직
- `net/session.cpp` - Lockstep 구현
- `core/rng.h` - 결정론적 RNG

**빌드 방법:**

```bash
# Windows
mingw32-make PLATFORM=PLATFORM_DESKTOP RAYLIB_PATH=C:/raylib/raylib

# Linux
mkdir -p build && cd build
cmake ..
cmake --build . -j
./tetris
```

**플레이 방법:**

```bash
# 호스트
./tetris --host 7777

# 클라이언트
./tetris --connect 127.0.0.1:7777
```
