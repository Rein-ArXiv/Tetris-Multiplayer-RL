# 네트워크 아키텍처 개요

Tetris-Multiplayer-RL 프로젝트의 네트워킹 시스템에 대한 종합적인 가이드입니다.

## 📋 목차

1. [전체 아키텍처](#전체-아키텍처)
2. [계층별 설계](#계층별-설계)
3. [Lockstep 동기화](#lockstep-동기화)
4. [메시지 프로토콜](#메시지-프로토콜)
5. [스레드 모델](#스레드-모델)
6. [에러 처리](#에러-처리)
7. [성능 고려사항](#성능-고려사항)

---

## 전체 아키텍처

### 네트워크 스택 구조

```
┌─────────────────────────────────────────────────────────────┐
│ Application Layer (main.cpp)                                │ ← 게임 로직, UI, 입력 처리
├─────────────────────────────────────────────────────────────┤
│ Session Layer (session.h/cpp)                              │ ← Lockstep 동기화, 세션 관리
├─────────────────────────────────────────────────────────────┤
│ Framing Layer (framing.h/cpp)                              │ ← 메시지 직렬화, 프로토콜
├─────────────────────────────────────────────────────────────┤
│ Socket Layer (socket.h/cpp)                                │ ← TCP 추상화, 플랫폼 독립성
├─────────────────────────────────────────────────────────────┤
│ OS Network Stack (WinSock/BSD Socket)                      │ ← 운영체제 네트워크 API
└─────────────────────────────────────────────────────────────┘
```

### 핵심 설계 원칙

1. **결정론적 게임플레이**: 같은 입력 → 같은 결과
2. **P2P Lockstep**: 모든 입력을 기다린 후 시뮬레이션 진행
3. **계층화된 추상화**: 각 계층이 독립적인 책임 수행
4. **플랫폼 독립성**: Windows/Linux 모두 지원
5. **멀티스레드 안전성**: 동시성 문제 방지

---

## 계층별 설계

### 1. Socket Layer (최하위 계층)

**목적**: 운영체제별 소켓 API 차이점 흡수

**주요 기능**:
- TCP 연결 설정/해제
- 논블로킹 I/O
- 플랫폼 독립적 인터페이스
- 에러 처리 및 재시도

**핵심 타입**:
```cpp
struct TcpSocket {
    int fd{-1};  // 소켓 파일 디스크립터
    bool valid() const { return fd >= 0; }
};
```

### 2. Framing Layer (프로토콜 계층)

**목적**: TCP 스트림에서 메시지 경계 구분

**프레임 구조**:
```
┌────────────┬─────────┬─────────────────┬─────────────┐
│ LENGTH     │ TYPE    │ PAYLOAD         │ CHECKSUM    │
│ 2 bytes    │ 1 byte  │ (LENGTH-1) bytes│ 4 bytes     │
└────────────┴─────────┴─────────────────┴─────────────┘
```

**메시지 타입**:
- `HELLO` (1): 프로토콜 버전 협상
- `HELLO_ACK` (2): HELLO 응답
- `SEED` (3): 게임 초기화 파라미터
- `INPUT` (4): 플레이어 입력 데이터
- `ACK` (5): 수신 확인
- `PING` (6): 연결 상태 확인
- `PONG` (7): PING 응답
- `HASH` (8): 게임 상태 검증

### 3. Session Layer (세션 관리 계층)

**목적**: Lockstep 동기화 및 세션 생명주기 관리

**핵심 상태**:
```cpp
enum class Role : uint8_t { Host=1, Peer=2 };

struct SeedParams {
    uint64_t seed{0};           // RNG 시드
    uint32_t start_tick{120};   // 시작 대기 틱
    uint8_t input_delay{2};     // 입력 지연 틱
    Role role{Role::Host};      // 네트워크 역할
};
```

### 4. Application Layer (게임 로직)

**목적**: 게임 규칙 구현 및 사용자 인터페이스

**주요 책임**:
- 60 FPS 게임 루프
- 사용자 입력 수집
- 렌더링 및 UI
- 세션 상태 관리

---

## Lockstep 동기화

### 알고리즘 개요

Lockstep은 모든 플레이어의 입력을 수집한 후에야 게임을 진행하는 동기화 방식입니다.

```
플레이어 A: [INPUT_A for tick N] ──┐
                                   ├─→ Wait for both inputs
플레이어 B: [INPUT_B for tick N] ──┘
                                   │
                                   ↓
                              Execute tick N
                              with both inputs
```

### 안전 틱 계산

```cpp
int64_t lastLocalSent = localTickNext - 1;
int64_t lastRemote = session.maxRemoteTick();
int64_t safeTickInclusive = min(lastLocalSent, lastRemote) - inputDelay;
```

**안전 틱**: 양쪽 플레이어의 입력이 확실히 도착한 마지막 틱

### 입력 지연 (Input Delay)

네트워크 지연을 흡수하기 위해 의도적으로 입력을 지연시킵니다:
- **지연 없음**: 네트워크 지터 시 게임 정지
- **지연 2틱**: 약 33ms 지연으로 안정성 확보
- **지연 4틱**: 약 67ms 지연으로 높은 안정성

---

## 메시지 프로토콜

### 연결 설정 프로토콜

```
Host                          Peer
 │                             │
 │ tcp_listen(7777)           │ tcp_connect(host, 7777)
 │ ←─────────────────────────── │
 │                             │
 │ ← HELLO (proto_ver=1) ───── │
 │ ─ HELLO_ACK ─────────────→  │
 │ ─ SEED (seed, delay...) ──→ │
 │                             │ ready = true
 │ ready = true               │
 │                             │
 │ ←──── Game INPUT ────────→  │ (Lockstep 시작)
```

### 메시지 상세 형식

#### HELLO 메시지
```cpp
struct HelloPayload {
    uint16_t protocol_version;  // 현재: 1
};
```

#### SEED 메시지
```cpp
struct SeedPayload {
    uint64_t seed;          // RNG 시드 (8 bytes)
    uint32_t start_tick;    // 시작 대기 틱 (4 bytes)
    uint8_t input_delay;    // 입력 지연 (1 byte)
    uint8_t role;          // Host=1, Peer=2 (1 byte)
};  // 총 14 bytes
```

#### INPUT 메시지
```cpp
struct InputPayload {
    uint32_t from_tick;     // 시작 틱 번호 (4 bytes)
    uint16_t count;         // 입력 개수 (2 bytes)
    uint8_t inputs[count];  // 입력 비트마스크들
};
```

입력 비트마스크:
```cpp
#define INPUT_NONE     0x00
#define INPUT_LEFT     0x01
#define INPUT_RIGHT    0x02
#define INPUT_DOWN     0x04
#define INPUT_ROTATE   0x08
#define INPUT_DROP     0x10
```

---

## 스레드 모델

### 스레드 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│ Main Thread (60 FPS)                                       │
│ • 게임 로직 실행                                            │
│ • 사용자 입력 수집                                          │
│ • 렌더링 및 UI                                              │
│ • Lockstep 동기화                                           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ↓ SendInput()
┌─────────────────────────────────────────────────────────────┐
│ I/O Thread                                                  │
│ • TCP 송수신 (논블로킹)                                     │
│ • 메시지 프레이밍                                           │
│ • 프로토콜 처리                                             │
│ • 에러 감지 및 복구                                         │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Accept Thread (Host만)                                      │
│ • 클라이언트 연결 대기                                      │
│ • 연결 수락 및 핸드셰이크                                   │
│ • I/O 스레드 시작                                           │
└─────────────────────────────────────────────────────────────┘
```

### 동기화 메커니즘

**Atomic 변수**:
```cpp
std::atomic<bool> connected{false};    // TCP 연결 상태
std::atomic<bool> ready{false};        // 게임 준비 상태
std::atomic<bool> quit{false};         // 스레드 종료 신호
std::atomic<uint32_t> lastRemoteTick{0}; // 원격 최신 틱
```

**Mutex 보호 영역**:
```cpp
std::mutex sendMu;      // 송신 큐 보호
std::mutex inMu;        // 수신 입력 맵 보호
```

---

## 에러 처리

### 네트워크 에러 분류

1. **연결 에러**
   - 연결 거부 (Connection refused)
   - 연결 시간 초과 (Connection timeout)
   - 호스트 도달 불가 (Host unreachable)

2. **전송 에러**
   - 부분 전송 (Partial send)
   - 송신 버퍼 가득참 (WOULDBLOCK)
   - 연결 끊어짐 (Connection reset)

3. **프로토콜 에러**
   - 체크섬 불일치
   - 알 수 없는 메시지 타입
   - 잘못된 프레임 길이

### 에러 복구 전략

**재시도 메커니즘**:
```cpp
// 논블로킹 소켓에서 WOULDBLOCK 처리
if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        continue;  // 재시도
    }
    return false;  // 실제 에러
}
```

**연결 상태 모니터링**:
```cpp
// 10초 연결 타임아웃
if (!ready.load()) {
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    if (elapsed > CONNECTION_TIMEOUT) {
        connectionFailed = true;
        quit = true;
    }
}
```

---

## 성능 고려사항

### CPU 최적화

1. **논블로킹 I/O**: 메인 스레드 블로킹 방지
2. **효율적인 메모리 관리**: 벡터 예약, 불필요한 복사 방지
3. **캐싱**: IP 주소 조회 결과 캐시
4. **짧은 대기 시간**: 활동 없을 때 2ms 슬립

### 네트워크 최적화

1. **작은 헤더**: 프레임 헤더 7바이트로 최소화
2. **배치 전송**: 여러 입력을 하나의 메시지로 묶음
3. **압축 없음**: 지연 시간 우선시
4. **TCP Nagle 비활성화 고려**: 실시간성 중시

### 메모리 최적화

1. **고정 크기 버퍼**: 동적 할당 최소화
2. **링 버퍼 고려**: 수신 버퍼 재사용
3. **스마트 포인터**: 자동 메모리 관리
4. **스택 할당**: 작은 임시 객체는 스택 사용

---

## 디버깅 도구

### 상태 해시 검증

```cpp
void SendHash(uint32_t tick, uint64_t hash);
bool GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const;
```

게임 상태의 해시를 주기적으로 교환하여 디싱크 감지:
```cpp
if (tick % 60 == 0) {  // 1초마다
    uint64_t myHash = game.ComputeStateHash();
    session.SendHash(tick, myHash);
}
```

### 로깅 시스템

```cpp
std::cout << "[NET] " << message << std::endl;     // 네트워크 이벤트
std::cout << "[LOCKSTEP] " << message << std::endl; // 동기화 상태
std::cout << "[DEBUG] " << message << std::endl;    // 일반 디버깅
```

### 연결 상태 표시

```cpp
DrawText(TextFormat("NET: %s", session.isConnected()?"CONNECTED":"DISCONNECTED"), 10, 580, 10, RAYWHITE);
DrawText(TextFormat("TICKS localSent=%u remoteMax=%u sim=%u",
         localTickNext-1, session.maxRemoteTick(), simTick), 10, 606, 10, RAYWHITE);
```

이 문서는 네트워킹 시스템의 전체적인 이해를 돕기 위한 개요입니다. 더 자세한 함수별 참조는 각 계층별 문서를 참고하세요.