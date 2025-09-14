# 프레이밍 프로토콜 상세 문서

TCP 스트림에서 메시지 경계를 구분하고 프로토콜을 정의하는 Framing Layer의 완전한 참조입니다.

## 📋 목차

1. [개요](#개요)
2. [프레임 구조](#프레임-구조)
3. [메시지 타입](#메시지-타입)
4. [함수 레퍼런스](#함수-레퍼런스)
5. [프로토콜 스펙](#프로토콜-스펙)
6. [구현 예제](#구현-예제)
7. [에러 처리](#에러-처리)
8. [성능 최적화](#성능-최적화)

---

## 개요

### TCP 스트림 문제와 해결책

**TCP의 근본적 문제**:
TCP는 "바이트 스트림" 프로토콜입니다. 이는 메시지 경계가 없다는 의미입니다.

```cpp
// 전송측에서 두 개의 메시지를 보냄
send(sock, "HELLO", 5);
send(sock, "WORLD", 5);

// 수신측에서 받을 수 있는 다양한 경우들:
recv() → "HELLOWORLD"    // 한 번에 모든 데이터
recv() → "HELL"          // 첫 번째 호출
recv() → "OWORLD"        // 두 번째 호출
recv() → "HE"            // 첫 번째 호출
recv() → "LLOWORLD"      // 두 번째 호출
```

**프레이밍의 해결책**:
각 메시지에 헤더를 추가하여 경계를 명확히 합니다.

```
원본 메시지: "HELLO", "WORLD"
프레이밍 후: [5|HELLO][5|WORLD]
           ↑     ↑
         길이   데이터
```

### 설계 목표

1. **메시지 경계 복원**: TCP 스트림에서 개별 메시지 구분
2. **데이터 무결성**: 전송 중 손상 감지
3. **플랫폼 독립성**: 엔디안 차이 해결
4. **확장성**: 새로운 메시지 타입 추가 용이
5. **효율성**: 최소한의 오버헤드

---

## 프레임 구조

### 바이너리 프레임 포맷

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
┌─────────────────────────────────┬─────────────────────────────────┐
│          LENGTH (16-bit)        │     MSG_TYPE     │              │
├─────────────────────────────────┼─────────────────────────────────┤
│                            PAYLOAD                                │
│                        (LENGTH - 1) bytes                        │
├─────────────────────────────────────────────────────────────────┤
│                        CHECKSUM (32-bit)                        │
└─────────────────────────────────────────────────────────────────┘
```

### 필드 상세 설명

#### LENGTH (2 bytes, Little-Endian)
- **범위**: 1 ~ 65535
- **의미**: MSG_TYPE + PAYLOAD의 총 길이
- **최소값**: 1 (MSG_TYPE만 있는 경우)
- **최대값**: 65535 (이론상 최대, 실제로는 더 작게 제한)

**계산 예시**:
```cpp
// HELLO 메시지 (payload 2바이트)
LENGTH = sizeof(MSG_TYPE) + payload.size() = 1 + 2 = 3

// INPUT 메시지 (payload 7바이트)
LENGTH = sizeof(MSG_TYPE) + payload.size() = 1 + 7 = 8
```

#### MSG_TYPE (1 byte)
메시지 종류를 나타내는 열거형 값:

```cpp
enum class MsgType : uint8_t {
    HELLO = 1,      // 연결 초기화
    HELLO_ACK = 2,  // HELLO 응답
    SEED = 3,       // 게임 파라미터
    INPUT = 4,      // 플레이어 입력
    ACK = 5,        // 수신 확인
    PING = 6,       // 생존 확인
    PONG = 7,       // PING 응답
    HASH = 8,       // 상태 검증
};
```

#### PAYLOAD (가변 길이)
메시지 타입별로 다른 구조를 가진 실제 데이터

#### CHECKSUM (4 bytes, Little-Endian)
PAYLOAD에 대한 FNV-1a 해시값 (데이터 무결성 검증용)

### 프레임 크기 계산

```
전체 프레임 크기 = 2(LENGTH) + 1(MSG_TYPE) + N(PAYLOAD) + 4(CHECKSUM)
                = 7 + N bytes

최소 프레임 크기 = 7 bytes (PAYLOAD가 0인 경우)
최대 프레임 크기 = 7 + 65534 = 65541 bytes (이론상)
```

---

## 메시지 타입

### 연결 관리 메시지

#### HELLO (타입 1)
**목적**: 프로토콜 버전 협상 및 연결 초기화

**Payload 구조**:
```cpp
struct HelloPayload {
    uint16_t protocol_version;  // 현재 버전: 1
};
```

**바이너리 표현** (Little-Endian):
```
[03 00] [01] [01 00] [XX XX XX XX]
 ↑      ↑    ↑       ↑
LENGTH TYPE PAYLOAD CHECKSUM
```

**사용 시점**:
- Client → Server: 연결 직후
- 버전 불일치 시 연결 거부

#### HELLO_ACK (타입 2)
**목적**: HELLO 메시지에 대한 응답

**Payload 구조**:
```cpp
struct HelloAckPayload {
    uint8_t status;  // 0=거부, 1=승인
};
```

**바이너리 표현**:
```
[02 00] [02] [01] [XX XX XX XX]
 ↑      ↑    ↑    ↑
LENGTH TYPE OK  CHECKSUM
```

#### SEED (타입 3)
**목적**: 게임 초기화 파라미터 전송 (Host → Peer)

**Payload 구조**:
```cpp
struct SeedPayload {
    uint64_t seed;          // RNG 시드 (8 bytes)
    uint32_t start_tick;    // 시작 대기 틱 수 (4 bytes)
    uint8_t input_delay;    // 입력 지연 틱 수 (1 byte)
    uint8_t role;          // Role::Host=1, Role::Peer=2 (1 byte)
};  // 총 14 bytes
```

**바이너리 표현**:
```
[0F 00] [03] [EF BE AD DE FE CA ...] [XX XX XX XX]
 ↑      ↑    ↑                       ↑
LENGTH TYPE  SEED(8) + TICK(4) +    CHECKSUM
              DELAY(1) + ROLE(1)
```

**예시 데이터**:
```cpp
// seed = 0xDEADBEEFCAFEBABE
// start_tick = 120
// input_delay = 2
// role = Host (1)

Payload: [BE BA FE CA EF BE AD DE 78 00 00 00 02 01]
```

### 게임 플레이 메시지

#### INPUT (타입 4)
**목적**: 플레이어 입력 데이터 전송

**Payload 구조**:
```cpp
struct InputPayload {
    uint32_t from_tick;     // 시작 틱 번호 (4 bytes)
    uint16_t count;         // 입력 개수 (2 bytes)
    uint8_t inputs[count];  // 입력 비트마스크 배열
};
```

**입력 비트마스크**:
```cpp
#define INPUT_NONE     0x00  // 입력 없음
#define INPUT_LEFT     0x01  // 좌로 이동
#define INPUT_RIGHT    0x02  // 우로 이동
#define INPUT_DOWN     0x04  // 빠르게 떨어뜨리기
#define INPUT_ROTATE   0x08  // 회전
#define INPUT_DROP     0x10  // 즉시 떨어뜨리기
```

**바이너리 표현 예시**:
```cpp
// 틱 100부터 3개의 입력: [NONE, LEFT, ROTATE]
[08 00] [04] [64 00 00 00 03 00 00 01 08] [XX XX XX XX]
 ↑      ↑    ↑                           ↑
LENGTH TYPE  FROM_TICK(100) COUNT(3)    CHECKSUM
              INPUTS[0,1,8]
```

**일반적인 사용 패턴**:
```cpp
// 단일 틱 입력 (가장 일반적)
from_tick = 100
count = 1
inputs = [INPUT_LEFT]

// 배치 입력 (네트워크 효율성을 위해)
from_tick = 100
count = 5
inputs = [INPUT_NONE, INPUT_LEFT, INPUT_LEFT, INPUT_ROTATE, INPUT_DROP]
```

#### ACK (타입 5)
**목적**: INPUT 메시지 수신 확인

**Payload 구조**:
```cpp
struct AckPayload {
    uint32_t up_to_tick;    // 이 틱까지 모든 입력을 받았음
};
```

**사용법**:
```cpp
// "틱 105까지의 모든 입력을 받았습니다"
[05 00] [05] [69 00 00 00] [XX XX XX XX]
 ↑      ↑    ↑             ↑
LENGTH TYPE  UP_TO_TICK   CHECKSUM
              (105)
```

### 연결 유지 메시지

#### PING (타입 6)
**목적**: 연결 상태 확인 및 레이턴시 측정

**Payload 구조**:
```cpp
struct PingPayload {
    uint64_t timestamp;     // 전송 시각 (8 bytes)
};
```

**사용 패턴**:
```cpp
// 5초마다 PING 전송
auto now = std::chrono::high_resolution_clock::now();
uint64_t timestamp = now.time_since_epoch().count();
```

#### PONG (타입 7)
**목적**: PING에 대한 응답

**Payload 구조**: PING과 동일 (timestamp 그대로 반환)

**레이턴시 계산**:
```cpp
// PING 전송 시점
uint64_t sendTime = getCurrentTimestamp();

// PONG 수신 후
uint64_t receiveTime = getCurrentTimestamp();
uint64_t roundTripTime = receiveTime - sendTime;
```

### 디버깅 메시지

#### HASH (타입 8)
**목적**: 게임 상태 동기화 검증

**Payload 구조**:
```cpp
struct HashPayload {
    uint32_t tick;          // 해시를 계산한 틱 (4 bytes)
    uint64_t hash_value;    // 게임 상태 해시 (8 bytes)
};
```

**사용 예시**:
```cpp
// 매 60틱(1초)마다 상태 해시 전송
if (tick % 60 == 0) {
    uint64_t myHash = game.ComputeStateHash();
    session.SendHash(tick, myHash);
}
```

**디싱크 감지**:
```cpp
uint32_t remoteTick;
uint64_t remoteHash;
if (session.GetLastRemoteHash(remoteTick, remoteHash)) {
    uint64_t myHash = game.ComputeStateHash();
    if (remoteTick == currentTick && remoteHash != myHash) {
        std::cout << "DESYNC 감지! 틱 " << remoteTick << std::endl;
        std::cout << "내 해시: 0x" << std::hex << myHash << std::endl;
        std::cout << "상대 해시: 0x" << std::hex << remoteHash << std::endl;
    }
}
```

---

## 함수 레퍼런스

### 해시 함수

#### `uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed=2166136261u)`

**목적**: FNV-1a 해시 알고리즘 구현

**매개변수**:
- `data`: 해시할 데이터의 시작 주소
- `len`: 해시할 바이트 수
- `seed`: 초기 해시값 (FNV-1a 표준값)

**반환값**: 32비트 해시값

**알고리즘**:
```cpp
uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed) {
    uint32_t hash = seed;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)data[i];      // XOR with byte
        hash *= 16777619;              // FNV prime
    }
    return hash;
}
```

**사용 예시**:
```cpp
std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
uint32_t checksum = net::fnv1a32(payload.data(), payload.size());
```

**FNV-1a의 특징**:
- **빠름**: 단순한 XOR과 곱셈 연산만 사용
- **분산성 양호**: 작은 변화에도 해시값이 크게 변함
- **비암호화**: 보안용이 아닌 무결성 검사용
- **결정론적**: 같은 입력은 항상 같은 출력

### 직렬화 함수

#### 쓰기 함수들

```cpp
void le_write_u16(std::vector<uint8_t>& v, uint16_t x);
void le_write_u32(std::vector<uint8_t>& v, uint32_t x);
void le_write_u64(std::vector<uint8_t>& v, uint64_t x);
```

**목적**: 정수를 리틀엔디안 바이트로 변환하여 벡터에 추가

**내부 동작**:
```cpp
void le_write_u32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xFF));         // 최하위 바이트
    v.push_back((uint8_t)((x >> 8) & 0xFF));  // 두 번째 바이트
    v.push_back((uint8_t)((x >> 16) & 0xFF)); // 세 번째 바이트
    v.push_back((uint8_t)((x >> 24) & 0xFF)); // 최상위 바이트
}
```

**사용 예시**:
```cpp
std::vector<uint8_t> buffer;
le_write_u32(buffer, 0x12345678);
// buffer = [0x78, 0x56, 0x34, 0x12]
```

#### 읽기 함수들

```cpp
uint16_t le_read_u16(const uint8_t* p);
uint32_t le_read_u32(const uint8_t* p);
uint64_t le_read_u64(const uint8_t* p);
```

**목적**: 리틀엔디안 바이트에서 정수로 변환

**내부 동작**:
```cpp
uint32_t le_read_u32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
```

**사용 예시**:
```cpp
uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
uint32_t value = le_read_u32(data);
// value = 0x12345678
```

### 프레임 빌드/파싱 함수

#### `std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload)`

**목적**: 메시지를 전송 가능한 프레임으로 직렬화

**매개변수**:
- `t`: 메시지 타입
- `payload`: 메시지 내용

**반환값**: 전송 준비된 프레임 바이트 배열

**내부 동작**:
```cpp
std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;

    // 1. LENGTH 필드 (TYPE + PAYLOAD 크기)
    uint16_t length = 1 + (uint16_t)payload.size();
    le_write_u16(frame, length);

    // 2. MSG_TYPE 필드
    frame.push_back((uint8_t)t);

    // 3. PAYLOAD 데이터
    frame.insert(frame.end(), payload.begin(), payload.end());

    // 4. CHECKSUM 필드 (PAYLOAD에 대한 해시)
    uint32_t checksum = fnv1a32(payload.data(), payload.size());
    le_write_u32(frame, checksum);

    return frame;
}
```

**사용 예시**:
```cpp
// HELLO 메시지 생성
std::vector<uint8_t> helloPayload;
le_write_u16(helloPayload, 1);  // protocol version

auto frame = net::build_frame(net::MsgType::HELLO, helloPayload);
// frame = [03 00 01 01 00 XX XX XX XX]
//          ^     ^  ^     ^
//        LEN   TYPE VER  CHECKSUM

// TCP로 전송
bool success = net::tcp_send_all(sock, frame.data(), frame.size());
```

#### `bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out)`

**목적**: TCP 스트림 버퍼에서 완성된 프레임들을 파싱

**매개변수**:
- `streamBuf`: 누적된 수신 바이트 (입출력, 파싱된 바이트는 제거됨)
- `out`: 파싱된 Frame 객체들을 추가할 배열

**반환값**: 파싱 성공 여부 (false = 심각한 오류)

**Frame 구조체**:
```cpp
struct Frame {
    MsgType type;                    // 메시지 타입
    std::vector<uint8_t> payload;    // 메시지 내용
};
```

**파싱 과정**:
```cpp
bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out) {
    while (streamBuf.size() >= 2) {  // 최소 LENGTH 필드 필요
        // 1. LENGTH 읽기
        uint16_t length = le_read_u16(streamBuf.data());

        // 2. 전체 프레임 도착 확인
        size_t totalFrameSize = 2 + length + 4;  // LENGTH + DATA + CHECKSUM
        if (streamBuf.size() < totalFrameSize) {
            return true;  // 더 기다림
        }

        // 3. MSG_TYPE과 PAYLOAD 추출
        uint8_t msgType = streamBuf[2];
        std::vector<uint8_t> payload(streamBuf.begin() + 3,
                                   streamBuf.begin() + 2 + length);

        // 4. CHECKSUM 검증
        uint32_t receivedChecksum = le_read_u32(streamBuf.data() + 2 + length);
        uint32_t calculatedChecksum = fnv1a32(payload.data(), payload.size());

        if (receivedChecksum != calculatedChecksum) {
            std::cerr << "체크섬 불일치!" << std::endl;
            // 프레임 건너뛰기
        } else {
            // 5. Frame 객체 생성
            Frame frame;
            frame.type = (MsgType)msgType;
            frame.payload = std::move(payload);
            out.push_back(std::move(frame));
        }

        // 6. 처리된 바이트 제거
        streamBuf.erase(streamBuf.begin(), streamBuf.begin() + totalFrameSize);
    }

    return true;
}
```

**사용 패턴**:
```cpp
std::vector<uint8_t> recvBuffer;
std::vector<net::Frame> frames;

// 게임 루프에서
while (gameRunning) {
    // TCP에서 데이터 수신
    if (!net::tcp_recv_some(sock, recvBuffer)) {
        break;  // 연결 종료
    }

    // 프레임 파싱
    if (net::parse_frames(recvBuffer, frames)) {
        // 파싱된 프레임들 처리
        for (const auto& frame : frames) {
            handleFrame(frame);
        }
        frames.clear();
    }
}
```

---

## 프로토콜 스펙

### 연결 설정 프로토콜

```
Client                                Server
  │                                     │
  │ ──── TCP connect ────────────────→  │
  │                                     │ tcp_accept()
  │ ──── HELLO(ver=1) ───────────────→  │
  │                                     │ 버전 확인
  │ ←─── HELLO_ACK(ok=1) ──────────────  │
  │                                     │
  │ ←─── SEED(params) ─────────────────  │ Host 역할
  │                                     │
  │ ready = true                       │ ready = true
  │                                     │
  │ ←────── INPUT/ACK ──────────────→   │ 게임 시작
```

### 게임 플레이 프로토콜

```
Player A                              Player B
  │                                     │
  │ ─── INPUT(tick=100, LEFT) ───────→  │
  │ ←── ACK(up_to=100) ──────────────   │
  │                                     │
  │ ←── INPUT(tick=100, ROTATE) ──────  │
  │ ─── ACK(up_to=100) ─────────────→   │
  │                                     │
  │ 두 입력 모두 도착                    │ 두 입력 모두 도착
  │ Lockstep 진행: 틱 100 실행          │ Lockstep 진행: 틱 100 실행
  │                                     │
  │ ─── INPUT(tick=101, NONE) ────────→ │
  │ ←── INPUT(tick=101, DROP) ───────   │
  │        ... 게임 계속 ...            │
```

### 오류 복구 프로토콜

```
Normal Flow                          Error Recovery
     │                                    │
     │ ─── INPUT(tick=100) ─────→         │ ─── INPUT(tick=100) ─────→
     │ ←── ACK(up_to=100) ──────          │ (패킷 손실)
     │                                    │
     │ ─── INPUT(tick=101) ─────→         │ ─── INPUT(tick=101) ─────→
     │                                    │ ←── ACK(up_to=99) ────── (틱 100 없음 감지)
     │                                    │
     │                                    │ ─── INPUT(tick=100) ─────→ (재전송)
     │                                    │ ←── ACK(up_to=101) ──────
```

---

## 구현 예제

### 간단한 채팅 프로그램

**메시지 타입 확장**:
```cpp
enum class ChatMsgType : uint8_t {
    HELLO = 1,
    CHAT_MESSAGE = 100,  // 사용자 정의 타입
    USER_JOIN = 101,
    USER_LEAVE = 102
};
```

**채팅 메시지 전송**:
```cpp
void sendChatMessage(const TcpSocket& sock, const std::string& message) {
    std::vector<uint8_t> payload;

    // 메시지 길이 (2바이트) + 메시지 내용
    le_write_u16(payload, (uint16_t)message.size());
    payload.insert(payload.end(), message.begin(), message.end());

    // 프레임 생성 및 전송
    auto frame = build_frame((MsgType)ChatMsgType::CHAT_MESSAGE, payload);
    tcp_send_all(sock, frame.data(), frame.size());
}
```

**채팅 메시지 수신**:
```cpp
void handleChatFrame(const Frame& frame) {
    if ((ChatMsgType)frame.type == ChatMsgType::CHAT_MESSAGE) {
        if (frame.payload.size() >= 2) {
            uint16_t msgLen = le_read_u16(frame.payload.data());
            if (frame.payload.size() >= 2 + msgLen) {
                std::string message(frame.payload.begin() + 2,
                                  frame.payload.begin() + 2 + msgLen);
                std::cout << "수신: " << message << std::endl;
            }
        }
    }
}
```

### 파일 전송 프로토콜

**대용량 데이터 처리**:
```cpp
enum class FileMsgType : uint8_t {
    FILE_START = 200,   // 파일 전송 시작
    FILE_CHUNK = 201,   // 파일 조각
    FILE_END = 202,     // 파일 전송 완료
};

struct FileStartPayload {
    uint64_t total_size;        // 전체 파일 크기
    uint16_t filename_len;      // 파일명 길이
    // filename 뒤따름
};

struct FileChunkPayload {
    uint32_t chunk_id;          // 청크 번호
    uint16_t chunk_size;        // 청크 크기
    // chunk data 뒤따름
};
```

**청크 기반 전송**:
```cpp
void sendFile(const TcpSocket& sock, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return;

    // 1. 파일 크기 확인
    file.seekg(0, std::ios::end);
    uint64_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // 2. FILE_START 전송
    std::vector<uint8_t> startPayload;
    le_write_u64(startPayload, fileSize);
    le_write_u16(startPayload, (uint16_t)filename.size());
    startPayload.insert(startPayload.end(), filename.begin(), filename.end());

    auto startFrame = build_frame((MsgType)FileMsgType::FILE_START, startPayload);
    tcp_send_all(sock, startFrame.data(), startFrame.size());

    // 3. FILE_CHUNK들 전송 (4KB씩)
    const size_t CHUNK_SIZE = 4096;
    uint32_t chunkId = 0;

    while (!file.eof()) {
        std::vector<char> buffer(CHUNK_SIZE);
        file.read(buffer.data(), CHUNK_SIZE);
        size_t bytesRead = file.gcount();

        if (bytesRead > 0) {
            std::vector<uint8_t> chunkPayload;
            le_write_u32(chunkPayload, chunkId++);
            le_write_u16(chunkPayload, (uint16_t)bytesRead);
            chunkPayload.insert(chunkPayload.end(), buffer.begin(),
                              buffer.begin() + bytesRead);

            auto chunkFrame = build_frame((MsgType)FileMsgType::FILE_CHUNK, chunkPayload);
            tcp_send_all(sock, chunkFrame.data(), chunkFrame.size());
        }
    }

    // 4. FILE_END 전송
    std::vector<uint8_t> endPayload;
    auto endFrame = build_frame((MsgType)FileMsgType::FILE_END, endPayload);
    tcp_send_all(sock, endFrame.data(), endFrame.size());
}
```

---

## 에러 처리

### 프레이밍 레벨 에러

1. **LENGTH 필드 오류**
   ```cpp
   if (length == 0 || length > 65534) {
       std::cerr << "잘못된 LENGTH: " << length << std::endl;
       return false;  // 연결 종료
   }
   ```

2. **알 수 없는 MSG_TYPE**
   ```cpp
   if (msgType < 1 || msgType > 8) {
       std::cerr << "알 수 없는 MSG_TYPE: " << (int)msgType << std::endl;
       // 해당 프레임 건너뛰기, 연결 유지
       continue;
   }
   ```

3. **체크섬 불일치**
   ```cpp
   if (receivedChecksum != calculatedChecksum) {
       std::cerr << "체크섬 불일치: " << std::hex
                 << "받은값=" << receivedChecksum
                 << ", 계산값=" << calculatedChecksum << std::endl;
       // 프레임 건너뛰기
   }
   ```

4. **메모리 부족**
   ```cpp
   if (length > MAX_FRAME_SIZE) {  // 예: 1MB
       std::cerr << "프레임 크기가 너무 큼: " << length << std::endl;
       return false;  // DoS 공격 방지
   }
   ```

### 복구 전략

**부분 수신 처리**:
```cpp
// TCP에서 프레임 일부만 도착한 경우
if (streamBuf.size() < totalFrameSize) {
    // 더 많은 데이터 대기
    return true;  // 에러 아님
}
```

**손상된 프레임 건너뛰기**:
```cpp
// 체크섬 오류 시 해당 프레임만 버리고 계속 진행
if (checksumError) {
    streamBuf.erase(streamBuf.begin(), streamBuf.begin() + totalFrameSize);
    continue;  // 다음 프레임 파싱
}
```

**연결 상태 모니터링**:
```cpp
// 일정 시간 동안 유효한 프레임이 없으면 연결 문제로 판단
auto lastValidFrame = std::chrono::steady_clock::now();

if (validFrameParsed) {
    lastValidFrame = std::chrono::steady_clock::now();
} else {
    auto elapsed = std::chrono::steady_clock::now() - lastValidFrame;
    if (elapsed > std::chrono::seconds(30)) {
        std::cerr << "장시간 유효한 데이터 없음 - 연결 종료" << std::endl;
        return false;
    }
}
```

---

## 성능 최적화

### 메모리 최적화

1. **벡터 예약**
   ```cpp
   std::vector<uint8_t> frame;
   frame.reserve(7 + payload.size());  // 정확한 크기 미리 할당
   ```

2. **이동 시맨틱 활용**
   ```cpp
   frame.payload = std::move(payload);  // 복사 대신 이동
   out.push_back(std::move(frame));
   ```

3. **불필요한 임시 객체 방지**
   ```cpp
   // 나쁜 예
   std::vector<uint8_t> temp = createPayload();
   frame.payload = temp;

   // 좋은 예
   frame.payload = createPayload();  // RVO 적용
   ```

### 네트워크 최적화

1. **배치 전송**
   ```cpp
   // 여러 작은 메시지를 모아서 한 번에 전송
   std::vector<uint8_t> batchBuffer;
   for (const auto& frame : framesToSend) {
       batchBuffer.insert(batchBuffer.end(), frame.begin(), frame.end());
   }
   tcp_send_all(sock, batchBuffer.data(), batchBuffer.size());
   ```

2. **압축 고려** (지연시간과 트레이드오프)
   ```cpp
   // LZ4나 zlib 같은 빠른 압축 알고리즘
   std::vector<uint8_t> compressed = compress(payload);
   if (compressed.size() < payload.size() * 0.8) {  // 20% 이상 절약시에만
       useCompressed = true;
   }
   ```

### 파싱 최적화

1. **링 버퍼 사용**
   ```cpp
   // 벡터 erase 대신 링 버퍼로 복사 비용 절약
   class RingBuffer {
       std::vector<uint8_t> buffer;
       size_t readPos = 0;
       size_t writePos = 0;
       // ... 구현
   };
   ```

2. **제로 카피 파싱**
   ```cpp
   // payload 복사 대신 포인터 + 길이로 참조
   struct Frame {
       MsgType type;
       const uint8_t* payloadPtr;  // 원본 버퍼 참조
       size_t payloadLen;
       // 주의: 원본 버퍼 수명 관리 필요
   };
   ```

3. **SIMD 해시 (고급)**
   ```cpp
   // AVX2를 이용한 병렬 해시 계산 (큰 payload에 유효)
   uint32_t fastHash(const uint8_t* data, size_t len);
   ```

이 문서는 Framing Layer의 모든 기능과 프로토콜 스펙을 다룹니다. 다음 단계로 Session Layer 문서를 참고하세요.