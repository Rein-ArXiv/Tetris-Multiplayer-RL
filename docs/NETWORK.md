# 테트리스 멀티플레이어 네트워크 아키텍처

## 개요

이 프로젝트는 **결정론적 P2P Lockstep** 방식으로 멀티플레이어 테트리스를 구현합니다. 네트워크는 3개 레이어로 구성되어 있습니다.

```
Application Layer    │  Game Logic (main.cpp)
                     │  ├─ 입력 샘플링
                     │  ├─ 틱 진행 제어
                     │  └─ 렌더링
Session Layer        │
                     │  P2P Session (session.h/cpp)
                     │  ├─ 연결 관리
                     │  ├─ 핸드셰이크
                     │  ├─ 입력 동기화
                     │  └─ 상태 검증
Framing Layer        │
                     │  Message Framing (framing.h/cpp)
                     │  ├─ 메시지 직렬화
                     │  ├─ 경계 복원
                     │  └─ 무결성 검사
Transport Layer      │
                     │  TCP Socket (socket.h/cpp)
                     │  ├─ 연결 설정
                     │  ├─ 데이터 송수신
                     │  └─ 플랫폼 추상화
```

## 네트워크 개념

### 1. 결정론적 게임 (Deterministic Game)

**왜 필요한가?**
- 동일한 입력 → 동일한 결과 보장
- 상태 불일치(desync) 방지
- 네트워크 대역폭 절약 (입력만 전송)

**구현 요소:**
- 고정 틱레이트 (60Hz): `TICKS_PER_SECOND = 60`
- 결정론적 RNG: 동일한 시드 → 동일한 블록 순서
- 상태 해시: 동기화 검증용 체크섬

### 2. Lockstep 동기화

**작동 원리:**
```
플레이어 A    플레이어 B
    │            │
    ├─ 입력 1 ────→ 수신 대기
    ├─ 수신 대기 ←── 입력 1
    │            │
    ├─ 틱 1 진행  │
    │         ├─ 틱 1 진행
    │            │
    ├─ 입력 2 ────→ 수신 대기
    ├─ 수신 대기 ←── 입력 2
```

**장점:**
- 완벽한 동기화 보장
- 구현이 단순함
- 치팅 감지 용이

**단점:**
- 네트워크 지연만큼 입력 지연 발생
- 한쪽이 느리면 전체가 대기

### 3. 입력 지연 버퍼

네트워크 지연을 고려하여 N틱만큼 여유를 둡니다:

```
현재 틱: 100
입력 지연: 4틱
안전 틱 = min(전송완료틱, 수신최신틱) - 4

전송완료: 100, 수신최신: 98
→ 안전틱 = min(100, 98) - 4 = 94틱까지 진행 가능
```

## 레이어별 상세 설명

### Transport Layer (socket.h/cpp)

**역할:**
- 운영체제 소켓 API 추상화
- TCP 연결 설정 및 데이터 송수신
- Windows/Linux 플랫폼 차이 흡수

**핵심 함수:**
- `net_init()`: WinSock 초기화 (Windows만)
- `tcp_listen()`: 서버 소켓 생성
- `tcp_accept()`: 클라이언트 연결 수락
- `tcp_connect()`: 서버 연결
- `tcp_send_all()`: 전체 데이터 송신 보장
- `tcp_recv_some()`: 논블로킹 수신

**논블로킹 I/O:**
```cpp
// 블로킹 방식 (문제)
recv(socket, buffer, size, 0);  // 데이터가 없으면 무한 대기

// 논블로킹 방식 (해결)
set_nonblocking(socket);        // 소켓을 논블로킹으로 설정
int n = recv(socket, buffer, size, 0);
if (n < 0 && errno == EAGAIN) {
    // 데이터 없음, 다른 작업 계속
}
```

### Framing Layer (framing.h/cpp)

**역할:**
- TCP 스트림에서 메시지 경계 구분
- 메시지 직렬화/역직렬화
- 무결성 검사

**프레임 구조:**
```
┌────────┬────────┬─────────────┬──────────┐
│ LEN    │ TYPE   │ PAYLOAD     │ CHECKSUM │
│ 2bytes │ 1byte  │ LEN-1 bytes │ 4bytes   │
└────────┴────────┴─────────────┴──────────┘

예시: HELLO 메시지 (프로토콜 버전 1)
LEN: 0x0003 (TYPE 1byte + PAYLOAD 2bytes)
TYPE: 0x01 (MsgType::HELLO)
PAYLOAD: 0x0001 (version 1)
CHECKSUM: FNV-1a(PAYLOAD)
```

**스트림 파싱:**
TCP는 메시지 경계가 없으므로 부분 수신을 고려해야 합니다:
```cpp
// 문제: "Hello"+"World" 전송 시 "Hell"+"oWorld" 수신 가능
// 해결: 길이 헤더로 메시지 경계 명시

while (streamBuf.size() >= 2) {  // LEN 필드 확인
    uint16_t len = read_u16(streamBuf);
    if (streamBuf.size() < 2 + len + 4) break;  // 전체 프레임 대기
    // 프레임 완성 → 파싱
}
```

### Session Layer (session.h/cpp)

**역할:**
- P2P 연결 관리 및 핸드셰이크
- 게임 입력 동기화
- 멀티스레드 I/O 처리

**스레드 구조:**
```
메인 스레드           │ I/O 스레드              │ Accept 스레드
                     │                        │ (호스트만)
게임 로직 실행        │ 네트워크 송수신          │ 연결 대기
입력 생성            │ 메시지 파싱             │ 클라이언트 수락
틱 진행 제어          │ 프레임 처리             │ 핸드셰이크 시작
렌더링               │ 송신 큐 처리            │
```

**핸드셰이크 시퀀스:**
```
호스트                          클라이언트
  │                              │
  ├─ tcp_listen(7777)            │
  ├─ accept() 대기 ──────────────── tcp_connect(host:7777)
  │                              │
  ├─ HELLO ─────────────────────→ │
  │                    ├─ HELLO_ACK
  ├─ SEED ──────────────────────→ │
  │                              ├─ ready = true
  ├─ ready = true                │
  │                              │
  ├─ 게임 시작                     ├─ 게임 시작
```

**입력 동기화:**
```cpp
// 매 틱마다 실행
session.SendInput(tick, inputMask);  // 내 입력 전송

// 안전 틱 계산
int safeTickInclusive = min(lastLocalSent, lastRemote) - inputDelay;

// 안전 틱까지만 진행
while (simTick <= safeTickInclusive) {
    uint8_t localInput, remoteInput;
    session.GetRemoteInput(simTick, remoteInput);

    gameLocal->SubmitInput(localInput);
    gameRemote->SubmitInput(remoteInput);

    gameLocal->Tick();
    gameRemote->Tick();
    simTick++;
}
```

## 메시지 프로토콜

### 연결 설정 메시지

**HELLO (Type: 1)**
```cpp
Payload: [프로토콜 버전: u16]
목적: 연결 확인 및 프로토콜 버전 협상
```

**HELLO_ACK (Type: 2)**
```cpp
Payload: [상태: u8]  // 1=OK, 0=Error
목적: HELLO 응답
```

**SEED (Type: 3)**
```cpp
Payload: [
    시드: u64,
    시작틱: u32,
    입력지연: u8,
    역할: u8
]
목적: 게임 파라미터 공유
```

### 게임 데이터 메시지

**INPUT (Type: 4)**
```cpp
Payload: [
    시작틱: u32,
    입력개수: u16,
    입력배열: u8[개수]
]
목적: 플레이어 입력 전송
예시: 틱 100부터 3개 입력 [0x01, 0x00, 0x04]
```

**HASH (Type: 8)**
```cpp
Payload: [
    틱: u32,
    해시: u64
]
목적: 게임 상태 동기화 검증
```

## 에러 처리 및 복구

### 네트워크 오류

1. **연결 끊김**
   ```cpp
   if (!tcp_recv_some(sock, recvBuf)) {
       // 연결 종료 → 게임 일시정지, 재연결 시도
   }
   ```

2. **프레임 손상**
   ```cpp
   if (checksum != calculated_checksum) {
       // 프레임 버림 → TCP가 재전송 보장
   }
   ```

3. **입력 누락**
   ```cpp
   if (!session.GetRemoteInput(tick, input)) {
       // 해당 틱 진행 불가 → 대기 또는 타임아웃
   }
   ```

### 동기화 오류

1. **상태 해시 불일치**
   ```cpp
   if (localHash != remoteHash) {
       // desync 감지 → 스냅샷 요청 또는 재시작
   }
   ```

2. **틱 진행 속도 차이**
   ```cpp
   // 입력 지연 동적 조정
   if (avgLatency > threshold) {
       inputDelay++;
   }
   ```

## 성능 고려사항

### 대역폭 최적화

1. **입력 압축**
   ```cpp
   // 현재: 매 틱 1바이트
   // 개선: 연속 틱을 묶어서 전송
   // 예: [틱100~105: 001100] → 6바이트를 1개 메시지로
   ```

2. **델타 인코딩**
   ```cpp
   // 이전 상태와의 차이만 전송
   // 블록 위치, 회전 등
   ```

### 지연 최적화

1. **예측 실행**
   ```cpp
   // 상대방 입력 예측 (이전 입력 기반)
   // 실제 입력 도착 시 되감기
   ```

2. **적응형 지연**
   ```cpp
   // 네트워크 상태에 따라 입력 지연 조정
   inputDelay = max(2, avgLatency * 1.5);
   ```

### CPU 최적화

1. **I/O 스레드 효율성**
   ```cpp
   // 활동이 없을 때 짧은 대기
   if (!hasActivity) {
       std::this_thread::sleep_for(1ms);
   }
   ```

2. **메모리 풀링**
   ```cpp
   // 메시지 객체 재사용
   // GC 압박 감소
   ```

## 확장 가능성

### 다중 플레이어 (4인 대전)

```cpp
// 현재: 1:1 P2P
// 확장: 1:N 스타 토폴로지 또는 전용 서버

class MultiSession {
    std::vector<Session> peers;  // 3명의 상대방

    void BroadcastInput(tick, mask);
    bool AllInputsReady(tick);
};
```

### 서버 권위 모델

```cpp
// P2P → 클라이언트-서버 전환
// 서버가 모든 입력 수집 후 상태 배포
// 치팅 방지 및 관전자 지원
```

### UDP 전환

```cpp
// 현재: TCP (신뢰성, 순서 보장)
// 확장: UDP + 커스텀 ARQ
// 더 낮은 지연, HOL 블로킹 해결
```

## 디버깅 도구

### 네트워크 로그

콘솔 출력으로 네트워크 상태를 실시간 확인:
```
[NET] Starting to listen on port 7777
[NET] Client connected!
[NET] Queued HELLO message
[NET] Sending packet of size 11
[NET] Received HELLO message
[NET] Host session is ready!
```

### 상태 해시 비교

`H` 키로 양쪽 게임 상태 해시를 출력:
```
Hash(single): 0x12345678 local: 0x12345678 remote: 0x12345678
```

### 틱 진행 모니터

화면 하단에 네트워크 상태 표시:
```
TICKS localSent=150 remoteMax=148 sim=144 delay=4
```

## 학습 포인트

이 구현을 통해 학습할 수 있는 네트워크 개념들:

1. **TCP vs UDP**: 신뢰성 vs 성능 트레이드오프
2. **블로킹 vs 논블로킹**: I/O 모델의 차이
3. **스레드 동기화**: atomic, mutex 활용
4. **프로토콜 설계**: 프레이밍, 상태 머신
5. **결정론적 시뮬레이션**: RNG, 고정 틱
6. **지연 처리**: 버퍼링, 예측, 보상
7. **에러 복구**: 타임아웃, 재전송, 상태 복원

실제 상용 게임에서는 더 복잡한 기법들이 사용되지만, 이 프로젝트는 기본 원리를 이해하기에 충분한 구현을 제공합니다.