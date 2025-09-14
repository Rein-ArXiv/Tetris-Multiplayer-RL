# Socket Layer 상세 문서

TCP 소켓 추상화 레이어의 완전한 참조 문서입니다.

## 📋 목차

1. [개요](#개요)
2. [핵심 개념](#핵심-개념)
3. [함수 레퍼런스](#함수-레퍼런스)
4. [사용 예제](#사용-예제)
5. [에러 처리](#에러-처리)
6. [플랫폼별 차이점](#플랫폼별-차이점)
7. [성능 고려사항](#성능-고려사항)

---

## 개요

Socket Layer (`net/socket.h`, `net/socket.cpp`)는 운영체제의 소켓 API를 얇게 감싸서 플랫폼 독립적인 네트워킹을 제공합니다.

### 주요 목표

1. **플랫폼 독립성**: Windows WinSock과 Linux BSD Socket API 통합
2. **타입 안전성**: 원시 정수 대신 TcpSocket 구조체 사용
3. **에러 처리**: 일관된 에러 처리 패턴 제공
4. **논블로킹 I/O**: 게임 루프 블로킹 방지
5. **리소스 관리**: RAII 패턴을 통한 안전한 소켓 정리

### TCP를 선택한 이유

**TCP의 장점**:
- ✅ **신뢰성**: 패킷 손실, 중복, 순서 바뀜 자동 해결
- ✅ **순서 보장**: 메시지가 보낸 순서대로 도착
- ✅ **흐름 제어**: 수신자 처리 속도에 맞춰 전송
- ✅ **단순함**: 복잡한 재전송 로직 불필요

**UDP 대비 단점**:
- ❌ **지연 시간**: 약간 더 높은 레이턴시
- ❌ **오버헤드**: 연결 설정 및 유지 비용
- ❌ **연결 상태**: 연결이 끊어질 수 있음

**Tetris에서 TCP가 적합한 이유**:
- 실시간성이 FPS만큼 중요하지 않음 (턴제에 가까움)
- 모든 입력이 정확히 전달되어야 함 (한 번의 입력 손실이 게임 파괴)
- 구현 복잡도 최소화 (교육용 프로젝트)

---

## 핵심 개념

### TcpSocket 구조체

```cpp
struct TcpSocket {
    int fd{-1};  // 소켓 파일 디스크립터
    bool valid() const { return fd >= 0; }
};
```

**필드 설명**:
- `fd`: 운영체제가 할당한 소켓 식별자
  - Windows: SOCKET 타입 (실제로는 unsigned int)
  - Linux: int 타입 (0, 1, 2는 stdin/stdout/stderr 예약)
  - -1: 유효하지 않은 소켓을 의미

**왜 래퍼 구조체를 사용하는가?**
1. **타입 안전성**: `int`와 소켓을 구분
2. **플랫폼 통일**: Windows/Linux 차이점 숨김
3. **확장성**: 나중에 추가 메타데이터 저장 가능
4. **디버깅**: 유효성 검사 메서드 제공

### 논블로킹 I/O

**블로킹 I/O의 문제점**:
```cpp
// 블로킹 방식 - 데이터가 올 때까지 무한 대기
int n = recv(sock, buffer, size, 0);  // 여기서 멈춤!
// 게임이 정지됨
```

**논블로킹 I/O의 해결**:
```cpp
// 논블로킹 방식 - 즉시 반환
int n = recv(sock, buffer, size, 0);
if (n < 0) {
    if (errno == EAGAIN) {
        // 데이터 없음 - 계속 진행
        return true;
    }
    // 실제 에러
    return false;
}
// n바이트 수신됨
```

---

## 함수 레퍼런스

### 시스템 초기화/정리

#### `bool net_init()`

**목적**: 네트워킹 시스템 초기화

**반환값**:
- `true`: 초기화 성공
- `false`: 초기화 실패

**플랫폼별 동작**:

**Windows**:
```cpp
WSADATA wsaData;
int r = WSAStartup(MAKEWORD(2,2), &wsaData);
return r == 0;
```

**Linux**:
```cpp
return true;  // 별도 초기화 불필요
```

**사용 시점**: 프로그램 시작 시 한 번만 호출

**예제**:
```cpp
int main() {
    if (!net::net_init()) {
        std::cerr << "네트워크 초기화 실패" << std::endl;
        return 1;
    }

    // 네트워크 작업...

    net::net_shutdown();
    return 0;
}
```

#### `void net_shutdown()`

**목적**: 네트워킹 시스템 정리

**플랫폼별 동작**:

**Windows**:
```cpp
WSACleanup();
```

**Linux**:
```cpp
// 별도 정리 불필요
```

**사용 시점**: 프로그램 종료 시

---

### 서버 소켓 함수

#### `TcpSocket tcp_listen(uint16_t port, int backlog=1)`

**목적**: 서버 소켓 생성 및 클라이언트 연결 대기 시작

**매개변수**:
- `port`: 대기할 포트 번호 (1024 이상 권장)
- `backlog`: 동시 연결 요청 대기 큐 크기

**반환값**:
- 성공: 유효한 `TcpSocket` (listen 소켓)
- 실패: `TcpSocket{-1}` (invalid 상태)

**내부 동작 순서**:
1. `socket()`: TCP 소켓 생성
2. `setsockopt(SO_REUSEADDR)`: 포트 재사용 설정
3. `bind()`: 포트에 바인딩
4. `listen()`: 연결 요청 대기 상태로 전환

**예제**:
```cpp
// 포트 7777에서 클라이언트 대기
auto serverSock = net::tcp_listen(7777, 5);
if (!serverSock.valid()) {
    std::cerr << "서버 소켓 생성 실패" << std::endl;
    return;
}
std::cout << "포트 7777에서 대기 중..." << std::endl;
```

**실패 원인**:
- **포트 이미 사용 중**: `Address already in use`
- **권한 부족**: 1024 미만 포트는 관리자 권한 필요
- **네트워크 어댑터 문제**: 네트워크 카드 비활성화
- **방화벽 차단**: Windows Defender 등이 포트 차단

#### `TcpSocket tcp_accept(const TcpSocket& server)`

**목적**: 클라이언트 연결 수락

**매개변수**:
- `server`: `tcp_listen()`으로 생성한 서버 소켓

**반환값**:
- 성공: 클라이언트와 통신할 수 있는 새 소켓
- 실패: `TcpSocket{-1}`

**내부 동작**:
1. `accept()`: 연결 요청을 블로킹 대기
2. `set_nonblocking()`: 수락된 소켓을 논블로킹으로 설정
3. 클라이언트 주소 정보 획득

**중요 특성**:
- **블로킹 함수**: 연결 요청이 올 때까지 대기
- **새 소켓 생성**: 원본 서버 소켓은 그대로 유지
- **자동 논블로킹 설정**: 반환된 소켓은 논블로킹 모드

**예제**:
```cpp
// 별도 스레드에서 실행 (블로킹 방지)
void acceptThread(TcpSocket serverSock) {
    auto clientSock = net::tcp_accept(serverSock);
    if (!clientSock.valid()) {
        std::cerr << "클라이언트 연결 실패" << std::endl;
        return;
    }

    std::cout << "클라이언트 연결됨!" << std::endl;
    // clientSock으로 통신...
}
```

---

### 클라이언트 소켓 함수

#### `TcpSocket tcp_connect(const std::string& host, uint16_t port)`

**목적**: 서버에 연결 시도

**매개변수**:
- `host`: 서버 주소 (IP 주소 또는 도메인명)
- `port`: 서버 포트 번호

**반환값**:
- 성공: 서버와 통신할 수 있는 소켓
- 실패: `TcpSocket{-1}`

**내부 동작**:
1. `getaddrinfo()`: 호스트명을 IP 주소로 변환
2. `socket()`: TCP 소켓 생성
3. `connect()`: 서버에 연결 시도 (3-way handshake)
4. `set_nonblocking()`: 소켓을 논블로킹으로 설정

**주소 변환 예제**:
```cpp
// 다음 주소들이 모두 가능:
tcp_connect("127.0.0.1", 7777);        // IPv4 주소
tcp_connect("localhost", 7777);         // 로컬호스트
tcp_connect("google.com", 80);          // 도메인명
tcp_connect("192.168.1.100", 7777);    // 로컬 네트워크
```

**실패 원인**:
- **Connection refused**: 서버가 해당 포트에서 대기하지 않음
- **Connection timed out**: 서버에 도달할 수 없음 (방화벽, 네트워크 문제)
- **Host unreachable**: 라우팅 문제 또는 잘못된 IP
- **DNS resolution failed**: 도메인명을 IP로 변환 실패

**예제**:
```cpp
// 로컬 서버에 연결
auto sock = net::tcp_connect("127.0.0.1", 7777);
if (!sock.valid()) {
    std::cerr << "서버 연결 실패" << std::endl;
    return;
}
std::cout << "서버에 연결됨!" << std::endl;
```

---

### 데이터 송수신 함수

#### `bool tcp_send_all(const TcpSocket& s, const void* data, size_t len)`

**목적**: 전체 데이터가 전송될 때까지 보장

**매개변수**:
- `s`: 송신할 소켓
- `data`: 전송할 데이터의 시작 주소
- `len`: 전송할 바이트 수

**반환값**:
- `true`: 모든 데이터 전송 성공
- `false`: 전송 실패 또는 연결 종료

**TCP 부분 전송 문제**:
```cpp
// 문제 상황
char data[1000] = "많은 데이터...";
int n = send(sock, data, 1000, 0);
// n은 1000이 아닐 수 있음! (예: n=600)
```

**해결 방법**:
```cpp
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;

    while (sent < len) {
        int n = send(s.fd, (const char*)(p + sent), (int)(len - sent), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 논블로킹에서 버퍼 가득참 - 잠시 대기
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            return false;  // 실제 에러
        }
        if (n == 0) return false;  // 연결 종료
        sent += (size_t)n;
    }
    return true;
}
```

**사용 예제**:
```cpp
std::string message = "Hello, World!";
bool success = net::tcp_send_all(sock, message.data(), message.size());
if (!success) {
    std::cerr << "메시지 전송 실패" << std::endl;
}
```

#### `bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf)`

**목적**: 수신 가능한 데이터를 논블로킹으로 읽어 버퍼에 추가

**매개변수**:
- `s`: 수신할 소켓
- `outBuf`: 데이터를 추가할 버퍼 (기존 데이터 유지)

**반환값**:
- `true`: 정상 동작 (데이터 있음/없음 무관)
- `false`: 연결 종료 또는 에러

**TCP 스트림 특성**:
```cpp
// 전송 측
send(sock, "HELLO", 5);
send(sock, "WORLD", 5);

// 수신 측에서 가능한 시나리오들:
recv() -> "HELLOWORLD" (10바이트 한번에)
recv() -> "HELL" (4바이트)
recv() -> "OWORLD" (6바이트)
recv() -> "HE" (2바이트)
recv() -> "LLOWORLD" (8바이트)
```

**내부 동작**:
```cpp
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf) {
    uint8_t tmp[4096];
    int n = recv(s.fd, (char*)tmp, sizeof(tmp), 0);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;  // 데이터 없음 - 정상
        }
        return false;  // 실제 에러
    }

    if (n == 0) {
        return false;  // 연결 종료
    }

    // 기존 버퍼 뒤에 새 데이터 추가
    outBuf.insert(outBuf.end(), tmp, tmp + n);
    return true;
}
```

**사용 패턴**:
```cpp
std::vector<uint8_t> recvBuffer;

// 게임 루프에서 지속적으로 호출
while (gameRunning) {
    if (!net::tcp_recv_some(sock, recvBuffer)) {
        std::cout << "연결 종료됨" << std::endl;
        break;
    }

    // recvBuffer에 누적된 데이터 처리
    processReceivedData(recvBuffer);

    // 처리된 데이터는 제거
    // (상위 레이어인 framing에서 담당)
}
```

#### `void tcp_close(TcpSocket& s)`

**목적**: 소켓 연결 종료 및 리소스 해제

**매개변수**:
- `s`: 닫을 소켓 (참조로 전달, fd가 -1로 설정됨)

**TCP 연결 종료 과정 (4-way handshake)**:
```
클라이언트         서버
    │               │
    │──── FIN ────→ │  "더 이상 보낼 데이터 없음"
    │ ←── ACK ────  │  "확인"
    │ ←── FIN ────  │  "서버도 더 이상 보낼 데이터 없음"
    │──── ACK ────→ │  "확인"
    │               │
  종료           종료
```

**내부 동작**:
```cpp
void tcp_close(TcpSocket& s) {
    if (!s.valid()) return;

#ifdef _WIN32
    closesocket(s.fd);
#else
    close(s.fd);
#endif

    s.fd = -1;  // 무효화
}
```

**안전한 정리 패턴**:
```cpp
class NetworkManager {
    TcpSocket sock;

public:
    ~NetworkManager() {
        if (sock.valid()) {
            net::tcp_close(sock);
        }
    }
};
```

---

### 유틸리티 함수

#### `std::string get_local_ip()`

**목적**: 로컬 네트워크 IP 주소 조회

**반환값**: 로컬 IP 주소 (예: "192.168.1.100") 또는 기본값 "127.0.0.1"

**사용 목적**:
- 같은 WiFi/LAN 내 기기들과 연결 시 사용
- 포트 포워딩 없이도 직접 연결 가능
- 외부 인터넷 트래픽 없이 고속 연결

**플랫폼별 구현**:

**Windows**:
```cpp
char hostname[256];
gethostname(hostname, sizeof(hostname));  // 컴퓨터 이름 획득
// getaddrinfo()로 첫 번째 IPv4 주소 반환
```

**Linux**:
```cpp
// Google DNS(8.8.8.8)에 가상 연결하여 로컬 소켓 주소 확인
int sock = socket(AF_INET, SOCK_DGRAM, 0);
connect(sock, 8.8.8.8:53);  // 실제 패킷 전송 안함
getsockname(sock, &addr);   // 로컬 소켓 주소 반환
```

**사용 예제**:
```cpp
std::string localIP = net::get_local_ip();
std::cout << "로컬 IP: " << localIP << std::endl;
std::cout << "친구에게 알려줄 주소: " << localIP << ":7777" << std::endl;
```

#### `std::string get_public_ip()`

**목적**: 공인 IP 주소 조회 (인터넷 연결용)

**반환값**: 공인 IP 주소 (예: "123.45.67.89") 또는 실패 시 빈 문자열

**동작 원리**:
1. 외부 웹 서비스에 HTTP 요청
2. 서버가 요청자의 공인 IP를 응답으로 반환
3. 응답에서 IP 주소만 파싱

**사용하는 서비스들** (장애 대응용 다중화):
```cpp
const char* services[] = {
    "https://api.ipify.org",        // 1순위
    "https://ipecho.net/plain",     // 2순위
    "https://icanhazip.com",        // 3순위
    nullptr
};
```

**플랫폼별 구현**:

**Windows (PowerShell)**:
```bash
powershell -Command "try { (Invoke-WebRequest -Uri 'https://api.ipify.org' -UseBasicParsing -TimeoutSec 3).Content } catch { '' }"
```

**Linux (curl)**:
```bash
curl -s --connect-timeout 3 https://api.ipify.org
```

**사용 시나리오**:
```cpp
std::string publicIP = net::get_public_ip();
if (!publicIP.empty()) {
    std::cout << "인터넷 연결용: " << publicIP << ":7777" << std::endl;
    std::cout << "주의: 포트 포워딩 필요!" << std::endl;
} else {
    std::cout << "공인 IP 조회 실패 - 로컬 네트워크만 사용 가능" << std::endl;
}
```

---

## 사용 예제

### 완전한 에코 서버/클라이언트

**서버 코드**:
```cpp
#include "net/socket.h"
#include <thread>
#include <iostream>

void handleClient(net::TcpSocket clientSock) {
    std::vector<uint8_t> buffer;

    while (true) {
        if (!net::tcp_recv_some(clientSock, buffer)) {
            std::cout << "클라이언트 연결 종료" << std::endl;
            break;
        }

        if (!buffer.empty()) {
            // 받은 데이터를 그대로 다시 전송 (에코)
            bool success = net::tcp_send_all(clientSock,
                                           buffer.data(), buffer.size());
            if (!success) {
                std::cout << "전송 실패" << std::endl;
                break;
            }

            std::cout << "에코: " << buffer.size() << " bytes" << std::endl;
            buffer.clear();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    net::tcp_close(clientSock);
}

int main() {
    if (!net::net_init()) {
        std::cerr << "네트워크 초기화 실패" << std::endl;
        return 1;
    }

    auto serverSock = net::tcp_listen(7777);
    if (!serverSock.valid()) {
        std::cerr << "서버 소켓 생성 실패" << std::endl;
        return 1;
    }

    std::cout << "포트 7777에서 대기 중..." << std::endl;

    while (true) {
        auto clientSock = net::tcp_accept(serverSock);
        if (clientSock.valid()) {
            std::cout << "클라이언트 연결됨" << std::endl;

            // 각 클라이언트를 별도 스레드에서 처리
            std::thread(handleClient, std::move(clientSock)).detach();
        }
    }

    net::tcp_close(serverSock);
    net::net_shutdown();
    return 0;
}
```

**클라이언트 코드**:
```cpp
#include "net/socket.h"
#include <iostream>
#include <string>

int main() {
    if (!net::net_init()) {
        std::cerr << "네트워크 초기화 실패" << std::endl;
        return 1;
    }

    auto sock = net::tcp_connect("127.0.0.1", 7777);
    if (!sock.valid()) {
        std::cerr << "서버 연결 실패" << std::endl;
        return 1;
    }

    std::cout << "서버에 연결됨. 메시지를 입력하세요:" << std::endl;

    std::string input;
    std::vector<uint8_t> recvBuffer;

    while (std::getline(std::cin, input)) {
        // 메시지 전송
        bool success = net::tcp_send_all(sock, input.data(), input.size());
        if (!success) {
            std::cout << "전송 실패" << std::endl;
            break;
        }

        // 에코 응답 대기
        bool responded = false;
        auto startTime = std::chrono::steady_clock::now();

        while (!responded) {
            if (!net::tcp_recv_some(sock, recvBuffer)) {
                std::cout << "연결 종료됨" << std::endl;
                goto cleanup;
            }

            if (recvBuffer.size() >= input.size()) {
                std::string echo(recvBuffer.begin(),
                               recvBuffer.begin() + input.size());
                std::cout << "에코: " << echo << std::endl;
                recvBuffer.erase(recvBuffer.begin(),
                               recvBuffer.begin() + input.size());
                responded = true;
            }

            // 타임아웃 체크
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > std::chrono::seconds(5)) {
                std::cout << "응답 타임아웃" << std::endl;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

cleanup:
    net::tcp_close(sock);
    net::net_shutdown();
    return 0;
}
```

---

## 에러 처리

### 일반적인 에러 코드

**Windows (WinSock)**:
```cpp
WSAEWOULDBLOCK      // 논블로킹에서 작업이 블로킹됨
WSAECONNRESET       // 연결이 상대방에 의해 재설정됨
WSAECONNABORTED     // 연결이 소프트웨어에 의해 중단됨
WSAETIMEDOUT        // 연결 시간 초과
WSAECONNREFUSED     // 연결이 거부됨
```

**Linux (errno)**:
```cpp
EAGAIN/EWOULDBLOCK  // 논블로킹에서 작업이 블로킹됨
ECONNRESET          // 연결이 상대방에 의해 재설정됨
ETIMEDOUT           // 연결 시간 초과
ECONNREFUSED        // 연결이 거부됨
EPIPE               // 파이프가 끊어짐 (상대방이 소켓을 닫음)
```

### 에러 처리 패턴

```cpp
bool safeNetworkOperation() {
    const int MAX_RETRIES = 3;

    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        auto sock = net::tcp_connect("server.com", 7777);
        if (sock.valid()) {
            return true;  // 성공
        }

        if (retry < MAX_RETRIES - 1) {
            std::cout << "연결 실패, " << (retry + 1) << "초 후 재시도..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(retry + 1));
        }
    }

    std::cerr << "모든 재시도 실패" << std::endl;
    return false;
}
```

---

## 플랫폼별 차이점

### 헤더 파일

**Windows**:
```cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")  // 링크 라이브러리
```

**Linux**:
```cpp
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
```

### 소켓 타입

**Windows**:
```cpp
SOCKET sock = socket(...);  // unsigned int
closesocket(sock);          // 종료 함수
```

**Linux**:
```cpp
int sock = socket(...);     // int
close(sock);                // 종료 함수
```

### 논블로킹 설정

**Windows**:
```cpp
u_long mode = 1;
ioctlsocket(fd, FIONBIO, &mode);
```

**Linux**:
```cpp
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

---

## 성능 고려사항

### TCP Nagle 알고리즘

기본적으로 TCP는 작은 패킷들을 모아서 보내는 Nagle 알고리즘을 사용합니다. 실시간 게임에서는 지연을 일으킬 수 있으므로 비활성화 고려:

```cpp
int flag = 1;
setsockopt(sock.fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
```

### 소켓 버퍼 크기 조정

```cpp
// 송신 버퍼 크기 증가
int sendBufSize = 65536;
setsockopt(sock.fd, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));

// 수신 버퍼 크기 증가
int recvBufSize = 65536;
setsockopt(sock.fd, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));
```

### Keep-Alive 설정

장시간 연결 유지를 위한 Keep-Alive:

```cpp
int keepAlive = 1;
setsockopt(sock.fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));
```

### 메모리 최적화

수신 버퍼 최적화:
```cpp
// 큰 버퍼 미리 예약
std::vector<uint8_t> recvBuffer;
recvBuffer.reserve(8192);  // 재할당 방지

// 주기적으로 크기 축소
if (recvBuffer.capacity() > 16384 && recvBuffer.size() < 4096) {
    std::vector<uint8_t>(recvBuffer).swap(recvBuffer);  // 크기 축소
}
```

이 문서는 Socket Layer의 모든 기능과 사용법을 다룹니다. 다음 단계로 Framing Layer 문서를 참고하세요.