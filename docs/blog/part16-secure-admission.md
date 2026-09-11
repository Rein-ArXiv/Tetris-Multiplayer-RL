# Part 16: 공개 접속 보호 — WSS와 일회용 게임 입장권

> **시리즈:** 제로부터 멀티플레이어 테트리스 + RL | [시리즈 목차](./README.md) | **Part 16**

---

## 이번 Part의 구현 계약

- **선행 상태:** [Part 6](part6-lockstep-networking.md)의 바이트 프레임과 `Session`, [Part 7](part7-relay-server.md)의 큐·룸, [Part 10](part10-meta-and-ranking.md)의 익명 계정·메타 API, [Part 14](part14-event-loop-scaling.md)의 reactor 릴레이가 동작한다. 표현 계층과 서버 검증 봇 BP는 [Part 15](part15-release-polishing.md)에서 분리했다.
- **이번 Part의 파일:** `meta/game_tickets.h`, `net/stream_transport.h`, `net/wss_client.h/.cpp`, `net/system_trust.h/.cpp`, `server/wss_gateway.cpp`를 추가한다. `meta/api_server.cpp`, `meta/http_client.*`, `net/socket.*`, `net/session.*`, `src/main.cpp`, 두 릴레이의 인증 진입점과 CMake·release·systemd·CI를 연결한다.
- **연결점:** 계정 토큰은 HTTPS API에서 게임 입장권으로 교환한다. 클라이언트는 기존 큐·룸 프레임의 자격 증명 칸에 입장권을 넣고 WSS로 보낸다. 게이트웨이는 TLS/WebSocket만 처리하고 내부 relay로 바이트를 넘긴다. relay가 meta에 입장권 소비를 요청한다.
- **완료 게이트:** 정상 인증서·입장권으로 입장하고 두 플레이어가 매칭·입력을 주고받아야 한다. 잘못된 인증서·출처·프레임·재사용 입장권·장기 계정 토큰은 거절한다. 접속 종료 후 작업 스레드와 연결 슬롯을 반환해야 한다. 공개 DNS·인증서 배치와 Windows/macOS 실기기 출시는 별도 검증이다.

---

## 1. HTTPS API만으로는 게임 연결이 보호되지 않는다

기존 클라이언트는 HTTPS로 게스트 계정을 만들 수 있었지만, 그 계정 토큰을
`QUEUE_JOIN`, `ROOM_CREATE`, `ROOM_JOIN`에 넣어 평문 TCP로 전송했다.
API에서 암호화한 비밀을 다른 연결에서 그대로 드러내는 셈이다. 토큰을 복사한 사람은
계정 기록과 상점에도 접근할 수 있으므로 단순한 게임 세션 번호로 취급할 수 없다.

해결은 두 층으로 나눈다. WSS가 전송 중 도청·변조를 막고, 일회용 입장권이
게임 서버에 전달하는 자격 증명의 용도와 수명을 좁힌다. 둘 중 하나만 넣으면
평문 입장권 선점이나 장기 토큰 노출 범위가 남는다. TLS 인증서 검증과 짧은 세션
자격 증명의 설계 근거는 [OWASP TLS 안내](https://cheatsheetseries.owasp.org/cheatsheets/Transport_Layer_Security_Cheat_Sheet.html)와
[Session Management 안내](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)를 참고한다.

완성된 연결은 다음과 같다. 두 공개 연결 모두 암호화하며 7777·8080은 내부 포트다.

```mermaid
flowchart LR
    C[네이티브 클라이언트] -->|HTTPS: 계정·상점·입장권| H[HTTPS API 프록시]
    H -->|loopback HTTP 8080| M[tetris_meta]
    C -->|WSS: 입장권·게임 프레임| W[tetris_wss_gateway]
    W -->|loopback TCP 7777| R[tetris_relay_reactor]
    R -->|secret: 입장권 소비·경기 결과| M
    M --> DB[(SQLite: 계정·BP·경기)]
    M --> T[메모리: 60초 입장권]
```

현재 서버는 Mac 하드웨어에서 실행하는 **Linux**다. OS가 macOS라는 뜻이 아니다.
Windows 예비 서버에는 같은 소스를 Windows용으로 빌드한다. WSS 게이트웨이는
epoll에 직접 의존하지 않으므로 Windows에도 같은 진입 구조를 둘 수 있다.

## 2. 계정과 입장권을 구분한다

| 자격 증명 | 보관 위치와 용도 | 수명 |
|---|---|---|
| 계정 토큰: 32자리 hex | 사용자 데이터 폴더, meta의 계정·상점 API | 기존 장기 토큰. 만료·회전·복구는 아직 없음 |
| 게임 입장권: `gt1.` + 32자리 hex | meta 메모리 → Session 작업 스레드 → relay 인증 | 발급 후 60초, 소비 1회 |
| relay secret | 서버 설정, meta의 내부 API 인증 | 운영자가 관리. 클라이언트 번들에 넣지 않음 |
| 봇 도전 티켓 | Part 15의 상대·seed·입력 재현 식별 | 별도 PvE 계약. 게임 입장권과 교환 불가 |

장기 토큰을 파일에서 없애거나 회원 가입을 추가하지 않는다. 사용자는 같은 익명 계정으로
들어가며, 서버에 입장할 때만 내부적으로 짧은 입장권을 교환한다.
`gt1.`은 버전과 목적을 드러내는 접두사다. 보안성은 접두사를 숨기는 데서 나오지 않고,
기존 `gen_token()`의 OS 암호학적 난수와 소비 시 검증에서 나온다.

### 2.1 발급과 소비 API

| 요청 | 인증 | 성공 | 실패 예 |
|---|---|---|---|
| `POST /v1/game-tickets`, `{token}` | 계정 토큰을 DB에서 확인 | `{ticket, expires_in:60}` | 401 무효 토큰, 429 발급 제한, 503 서버 설정/난수/용량 |
| `POST /v1/game-tickets/consume`, `{ticket}` | `X-Relay-Secret` 상수 시간 비교 | player ID·이름·RP·BP·XP·아이콘 | 403 secret 오류, 401 만료·재사용·없는 입장권 |

발급은 IP당 60초에 10회다. 기존 HTTP 요청 예산과 작업자·본문 크기 제한도 적용한다.
JSON 응답에는 `Cache-Control: no-store`를 붙인다. 입장권을 URL 쿼리로 넘기지 않는다.
게이트웨이도 `/play?token=...`를 받지 않는다.

소비 라우트는 응답을 얻기 전에 입장권을 삭제한다.

**현재 소스 발췌 — `meta/api_server.cpp`**

```cpp
    svr.Post("/v1/game-tickets/consume", [&](const httplib::Request& req, httplib::Response& res) {
        if (relay_secret_.empty() || !ct_equal(req.get_header_value("X-Relay-Secret"), relay_secret_)) {
            set_json(res, 403, proto::error_json("relay_auth_required")); return;
        }
        auto auth = gameTickets.consume(proto::find_string(req.body, "ticket"));
        if (!auth) { set_json(res, 401, proto::error_json("invalid_game_ticket")); return; }
        set_json(res, 200, *auth);
    });
```

TLS는 API 프록시가 담당한다. `tetris_meta` 자체가 HTTPS 리스너로 바뀐 것은 아니다.
일반 네이티브 `MetaClient`는 원격 HTTP URL을 거절하고, 로컬 개발용
`localhost`·`127.0.0.1`·`::1`만 예외로 둔다. relay의 secret을 가진 내부 API
클라이언트는 사설 HTTP를 쓸 수 있지만, 다른 호스트라면 보호된 사설망/VPN 또는
HTTPS로 배치해야 한다. 임의의 인터넷 HTTP 주소를 내부망으로 간주하지 않는다.

### 2.2 원자적 소비 — mutex가 보호하는 범위

조회할 때만 잠그고 나중에 지우면 두 relay가 동시에 같은 티켓을 승인받을 수 있다.
따라서 조회·삭제·만료 판정을 한 임계 구역에 둔다.

**현재 소스 발췌 — `meta/game_tickets.h`**

```cpp
    std::optional<std::string> consume(const std::string& ticket,
                                      Clock::time_point now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(ticket);
        if (it == entries_.end()) return std::nullopt;
        auto entry = std::move(it->second);
        entries_.erase(it); // Burn even an expired ticket; never cache successful redemptions.
        if (entry.expires <= now) return std::nullopt;
        return entry.auth;
    }
```

시계는 `steady_clock`이다. 시스템 날짜 보정이 입장권 수명을 늘리지 않게 한다.
시험에서는 `now`를 주입해 60초 경계의 만료를 실제로 기다리지 않고 검사한다.

`issue()`는 만료 항목과 같은 플레이어의 이전 입장권을 정리한 뒤 새 항목을 넣는다.
전체 최대 4096개, 계정당 대기 입장권 1개다. 새 티켓을 받으면 아직 쓰지 않은
이전 티켓은 무효가 된다. meta 재시작도 미사용 티켓을 모두 무효화한다.
입장권은 DB에 저장하지 않으므로 서버 이전 시 복사할 필요가 없다.

항목에는 장기 토큰 대신 player ID와 발급 당시 인증 응답을 저장한다.
RP·선택 아이콘은 최대 60초 전 값일 수 있다. 이것은 입장 표시용 스냅샷이며,
상점 잔액을 이 값으로 덮어쓰지 않는다. 실제 구매·결과 반영은 계속 DB가 결정한다.

### 2.3 응답을 잃었을 때는 새 입장권을 받는다

meta가 소비를 끝낸 직후 응답이 유실될 수 있다. relay는 소비 요청을 자동 재시도하지
않으며 성공 응답을 캐시하지 않는다. 그 접속은 거절하고, 사용자의 다음 접속 시도에서
새 입장권을 발급한다. 연결이 잠깐 끊기는 불편을 감수하고 “한 번만 승인” 계약을 지킨다.

기존 thread relay의 `cached_auth`·`cache_auth`와 5분 오프라인 인증 허용을 제거했다.
reactor도 같은 `consume_game_ticket()`을 호출한다. meta 장애 때 이미 진행하는 경기의
바이트 전달이 즉시 중단되는 것은 아니지만, 새 인증 입장은 실패한다.

## 3. 클라이언트 연결 경로를 바꾼다

### 3.1 계정 토큰을 보내기 직전에 교환한다

`src/main.cpp`가 `Session::SetTicketIssuer`에 `MetaClient::request_game_ticket`을
연결한다. `Session`이 meta 구현 전체를 직접 소유하지 않도록 콜백으로 주입했다.
콜백은 세션 작업을 시작하기 전에 설정하고 세션이 종료될 때까지 유지한다.

`roomThread()`와 `queueThread()`가 다음 함수를 호출한다. HTTP 왕복과 TLS 접속은
화면 렌더링 스레드에서 기다리지 않는다.

**현재 소스 발췌 — `net/session.cpp`**

```cpp
bool Session::prepareGameCredential(const std::string& host, std::string& credential) {
    if (credential.empty()) return true; // local unranked play
    if (!secure_game_endpoint(host)) return false;
    if (credential.rfind("gt1.",0)==0) return true;
    if (!ticketIssuer_) return false;
    auto ticket = ticketIssuer_(credential);
    credential.clear(); // never write the account credential to the relay
    if (!ticket) return false;
    credential = std::move(*ticket);
    return true;
}
```

인증 정보가 있는 원격 게임 접속은 유효한 `wss://.../play` 주소여야 한다.
빈 자격 증명을 쓰는 기존 연습용 TCP와 명시적 loopback 개발 경로는 남겨 둔다.
입장권 발급 실패를 장기 토큰 직접 전송으로 우회하지 않는다.

접속 순서는 다음과 같다.

```mermaid
sequenceDiagram
    participant U as main / 메뉴
    participant S as Session 작업 스레드
    participant M as meta HTTPS API
    participant G as WSS 게이트웨이
    participant R as 내부 relay
    U->>S: 큐 또는 방 입장
    S->>M: 계정 토큰으로 입장권 요청
    M-->>S: 60초 gt1 입장권
    S->>G: 인증서 검증 후 WSS 연결
    S->>G: 기존 큐·룸 프레임 + 입장권
    G->>R: 바이트 전달
    R->>M: secret + 입장권 소비
    M-->>R: 승인된 player 정보
    R-->>S: ROOM_INFO 또는 MATCH_FOUND
```

인증된 player ID의 중복 활성 세션 차단은 기존 `PlayerSessionLease`가 계속 맡는다.
입장권을 소비했어도 중복 세션으로 거절될 수 있다. 이때도 사용한 입장권은 되살리지 않는다.

### 3.2 바이트 프레임과 전송 수단을 분리한다

`TcpSocket`은 기존 FD 외에 선택적 `shared_ptr<StreamTransport>`를 갖는다.
`tcp_send_all`, `tcp_recv_some`, `tcp_close`, `valid()`가 이 객체를 먼저 확인한다.
따라서 `Session`의 큐·룸·게임 프레임 코드는 같은 함수를 쓸 수 있다.
`game_connect()`만 WSS 주소와 기존 TCP 주소를 구분한다.

| 모듈 | 책임 |
|---|---|
| `net/framing.*` | 기존 LEN/TYPE/PAYLOAD/FNV 프레임. 형식 변경 없음 |
| `net/session.*` | 큐·룸 상태, 60Hz 입력, 입장권 교환 시점 |
| `net/stream_transport.h` | 살아 있는지, 보내기, 받은 바이트 꺼내기, 종료 |
| `net/wss_client.cpp` | 주소 검증, TLS, WebSocket, 비동기 I/O와 버퍼 |
| `net/socket.cpp` | 기존 TCP 처리 또는 WSS 전송 객체로 위임 |
| `server/wss_gateway.cpp` | WSS ↔ loopback TCP 변환 |
| `server/player_conn.cpp`, `server/reactor_relay.cpp` | 첫 프레임에서 입장권 소비, 계정 lease 확보 |

WebSocket 메시지 하나가 Tetris 프레임 하나와 같다고 가정하지 않는다.
TCP는 임의 위치에서 끊겨 도착하므로 게이트웨이가 보내는 메시지에도 프레임 일부나
여러 프레임이 들어갈 수 있다. 수신 측은 받은 바이트를 기존 stream 버퍼에 모아 파싱한다.
TLS는 전송 무결성을 추가하며, FNV 체크섬을 암호학적 인증으로 격상시키지 않는다.
작은 60Hz INPUT을 불필요하게 모아서 보내지 않도록 클라이언트·gateway 양쪽
TCP 구간에는 기존 raw 경로처럼 `TCP_NODELAY`도 설정한다.

reactor가 소유한 FD는 계속 실제 TCP다. WSS 객체를 IOCP/epoll에 억지로 등록하지 않고
게이트웨이 프로세스로 TLS 처리를 분리했기 때문에 기존 샤딩 계약을 유지한다.

## 4. TLS와 비동기 I/O의 수명

### 4.1 인증서를 검사하는 것이 암호화만큼 중요하다

`WssClient`는 OpenSSL의 `verify_peer`, 호스트 이름 검증, SNI를 사용한다.
최소 TLS 버전은 1.2다. 신뢰하지 않는 발급자, 다른 호스트의 인증서, 만료된
인증서는 연결 실패다. 검증을 끄는 실행 옵션은 제공하지 않는다.

Linux는 OpenSSL 기본 CA 경로, Windows는 시스템 인증서 저장소, macOS는 Keychain의
인증서를 사용한다. `net/system_trust.cpp`가 이미 vendored cpp-httplib에 있는 플랫폼
어댑터 호출을 한 파일에 격리한다. 이 어댑터는 라이브러리 내부 함수에 의존하므로
cpp-httplib를 업데이트할 때 세 OS 컴파일·인증서 검사를 함께 실행한다.

`TETRIS_CA_FILE=/path/to/ca.pem`은 개발 또는 사설 CA를 추가하는 설정이다.
인증서 검사를 생략하는 설정이 아니다. 공개 서버에는 정상 체인과 도메인이 필요하다.
CA 저장소는 실행 파일에 고정 복사하지 않고 운영체제에서 계속 갱신할 수 있게 둔다.

### 4.2 하나의 TLS 스트림을 여러 스레드가 직접 만지지 않는다

Beast의 WebSocket은 동기식 논블로킹 호출을 제공하지 않는다. 기존 TCP의 `send`/`recv`
폴링을 SSL 스트림에 그대로 이식하면 호출 순서와 대기 처리가 복잡해진다.
그래서 클라이언트 연결마다 전용 `io_context`와 작업 스레드를 둔다.
[Beast의 사용상 제약](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/using_websocket/notes.html)에 맞춰
한 read와 한 write만 동시에 진행하고 쓰기는 순서대로 직렬화한다.

- Session의 send는 바이트를 복사해 I/O 스레드에 게시한다. 전송 대기 총량은 128 KiB다.
- read 완료는 mutex로 보호된 수신 버퍼에 넣는다. 누적 상한도 128 KiB다.
- Session의 기존 폴링은 이 수신 버퍼만 비운다. SSL 객체를 다른 스레드에서 읽지 않는다.
- 연결 설정 전체에는 5초 마감, WebSocket에는 30초 idle 설정과 keepalive를 둔다.
- WebSocket 메시지는 최대 16 KiB, binary만 허용한다. 압축 기능은 템플릿에서 제외한다.

정상 접속 검사에서 종료가 10초 이상 멈추는 오류가 발견됐다. 소켓을 닫아도
Beast의 idle 타이머가 남아 전용 I/O 스레드의 `join()`이 기다리는 것이 원인이었다.
종료 시 resolver·타이머·소켓을 취소하고 **이 연결만 사용하는** `io_context`도 멈춘다.
그 다음 외부 소유자가 스레드를 join하고 스트림을 파괴한다. 공유 서버 루프 전체를
멈추는 방식으로 이 코드를 옮겨 쓰면 다른 연결까지 종료되므로 소유 범위를 유지해야 한다.

## 5. 게이트웨이를 무제한 중계기로 만들지 않는다

`server/wss_gateway.cpp`는 Boost.Beast의 RFC 6455 구현과 OpenSSL을 사용한다.
별도 WebSocket 파서나 암호 알고리즘을 직접 만들지 않는다.

| 경계 | 현재 제한과 의도 |
|---|---|
| 목적지 | 항상 `127.0.0.1:backend-port`. 요청에서 목적지 선택 불가 |
| 공개 경로 | 정확히 `/play`. 쿼리·다른 경로 거절 |
| TLS·HTTP 설정 | 인증서/개인키 필수, 설정 단계 5초, HTTP 헤더 8 KiB, 요청 본문 0 |
| 브라우저 Origin | 명시한 HTTPS origin과 정확히 일치. 빈 값·`null`·중복·미허용 값 거절 |
| 네이티브 | Origin 생략 허용. 계정 인증은 여전히 relay가 입장권으로 수행 |
| 메시지 | binary 최대 16 KiB, 마스킹/분할/제어 프레임은 Beast가 처리, 압축 미협상 |
| 데이터 속도 | 연결당 초당 64 KiB, 순간 여유 128 KiB |
| 연결 수 | 전체 128개, 공인 IPv4 또는 IPv6 /64당 동시 16개 |
| 전달 버퍼 | 방향별 전달이 끝나야 다음 read. 느린 수신자 앞에서 무한 큐를 만들지 않음 |

Origin 검사는 브라우저가 의도하지 않은 사이트에서 연결하는 경로를 좁힌다.
네이티브 프로그램은 Origin을 생략하거나 꾸밀 수 있으므로 사용자 인증 수단은 아니다.
[OWASP WebSocket 안내](https://cheatsheetseries.owasp.org/cheatsheets/WebSocket_Security_Cheat_Sheet.html)도
출처 확인과 메시지·접속 제한을 별도 방어층으로 다룬다.

게이트웨이는 실제 소켓 peer 주소를 사용하고 XFF 같은 전달 헤더를 신뢰하지 않는다.
따라서 이 예제에서는 게이트웨이가 외부 TLS 연결을 직접 받는다. 다른 프록시 뒤에
둘 경우 모든 접속이 프록시 IP 한 개의 16개 제한을 공유할 수 있다. 그 배치를 쓰려면
신뢰할 프록시 식별과 원래 주소 전달을 별도로 구현·검증해야 한다.

relay에는 게이트웨이 연결이 모두 loopback으로 보인다. 예제 서비스는
`--loopback-only --max-sessions-per-ip 128`을 사용한다. 인터넷 주소별 제한은 게이트웨이,
내부 총량과 기존 handshake 예산은 relay가 맡는다. 7777을 공개하면 이 분리가 깨진다.
이 상한은 시작값이며 DDoS 방어나 검증된 운영 동접 수치가 아니다.

## 6. 빌드와 실행 — 무엇부터 켜는가

### 6.1 빌드 옵션과 의존성

기본 개발 빌드의 `TETRIS_BUILD_WSS`는 OFF다. 기존 규칙·로컬 TCP 실습은 Boost 없이
가능하게 유지한다. 공개 연결용 빌드와 release 스크립트는 WSS를 명시적으로 켠다.
OpenSSL 개발 라이브러리와 Boost.Beast 헤더(Boost 1.74 이상을 기준으로 준비)가 필요하다.
WSS 기본 주소를 넣으면서 WSS를 끄면 CMake가 구성을 거절한다.

현재 최종 소스에 해당하는 의존성 연결은 다음과 같다. `tetris_wss_deps`는 공통
include·define·TLS·스레드 라이브러리를 클라이언트와 게이트웨이에 전달한다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
    add_library(tetris_wss_deps INTERFACE)
    target_include_directories(tetris_wss_deps INTERFACE "${TETRIS_BOOST_INCLUDE}")
    target_compile_definitions(tetris_wss_deps INTERFACE TETRIS_HAS_WSS=1 BOOST_ERROR_CODE_HEADER_ONLY)
    target_link_libraries(tetris_wss_deps INTERFACE OpenSSL::SSL OpenSSL::Crypto Threads::Threads tetris_system_trust)
```

Linux에서 서버와 그래픽 클라이언트를 함께 확인하려면 저장소 루트에서 실행한다.
서버만 필요한 장비는 `TETRIS_BUILD_GAME=OFF`로 하고 SDL/OpenGL 준비를 생략한다.

```bash
sudo apt install build-essential cmake libboost-dev libssl-dev libsdl2-dev libgl1-mesa-dev
cmake -S . -B build-secure -DCMAKE_BUILD_TYPE=Release \
  -DTETRIS_BUILD_GAME=ON -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_META=ON \
  -DTETRIS_BUILD_WSS=ON -DTETRIS_ENABLE_HTTPS=ON \
  -DTETRIS_BUILD_TEST=ON -DTETRIS_BUILD_PY=OFF -DTETRIS_BUILD_BOT=OFF
cmake --build build-secure --parallel 3
```

Boost가 표준 경로에 없으면 `-DTETRIS_BOOST_INCLUDE=/path/to/boost/include`를 추가한다.
실제 헤더는 그 아래 `boost/beast.hpp`에 있어야 한다. macOS는
`brew install cmake sdl2 boost openssl@3` 후 `-DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"`를 준다.
macOS reactor는 제외하고 portable relay를 사용한다.

Windows는 Visual Studio C++ 도구와 vcpkg를 준비한 뒤 PowerShell에서 구성한다.
아래는 `VCPKG_ROOT`가 실제 vcpkg 설치 폴더를 가리키는 경우다.

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install boost-beast:x64-windows openssl:x64-windows
cmake -S . -B build-secure -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DTETRIS_BUILD_WSS=ON -DTETRIS_ENABLE_HTTPS=ON -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_META=ON -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_TEST=ON -DTETRIS_BUILD_PY=OFF
cmake --build build-secure --config Release --parallel 3
```

### 6.2 로컬에서 TLS 연결 확인

다음 인증서는 테스트 전용이다. 공개 배포 인증서로 복사하지 않는다.
별도 출력 디렉터리에 만들고 서버 프로세스는 각각의 터미널에서 실행한다.
기존 사용자 데이터와 충돌하지 않도록 테스트용 데이터 폴더를 사용한다.

```bash
mkdir -p out/local-tls
umask 077
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -addext subjectAltName=DNS:localhost \
  -keyout out/local-tls/key.pem -out out/local-tls/cert.pem
```

먼저 Part 10에서 준비한 같은 `TETRIS_RELAY_SECRET`을 meta와 relay 터미널에 읽힌다.
계정 DB와 secret은 배포 에셋 폴더에 넣지 않는다.

```bash
# 터미널 A: 계정·입장권
./build-secure/tetris_meta --http 127.0.0.1:8080 --db out/local-tls/meta.db
```

```bash
# 터미널 B: 내부 게임 서버
./build-secure/tetris_relay_reactor --port 7777 --loops 1 --loopback-only \
  --max-sessions-per-ip 128 --meta http://127.0.0.1:8080
```

```bash
# 터미널 C: TLS 게임 진입점
./build-secure/tetris_wss_gateway --port 8443 --backend-port 7777 \
  --cert out/local-tls/cert.pem --key out/local-tls/key.pem
```

```bash
# 터미널 D: 두 번째 플레이어는 player-a를 player-b로 바꾼다.
TETRIS_CA_FILE="$PWD/out/local-tls/cert.pem" XDG_DATA_HOME="$PWD/out/local-tls/player-a" \
  ./build-secure/tetris --relay wss://localhost:8443/play --meta http://127.0.0.1:8080
```

인증서의 이름은 `localhost`이므로 게임 주소를 `wss://127.0.0.1:8443/play`로
바꾸면 호스트 검증이 실패하는 것이 정상이다. 종료 순서는 게임 → 게이트웨이 → relay → meta다.

### 6.3 실제 배포와 Windows 이전

운영 클라이언트에는 `wss://relay.example.com:8443/play`과
`https://api.example.com`을 설정한다. 게임 게이트웨이의 인증서 체인·개인키와
API 프록시의 HTTPS를 각각 준비한다. 게임 도메인 인증서 갱신 후에는 게이트웨이를
재시작해야 한다. 현재 인증서 hot reload는 없으므로 진행 경기 종료와 점검 시간을 고려한다.

`deploy/systemd/tetris-wss.service`는 Linux용 예제다. 인증서 경로와 Origin을 바꾸고,
서비스 사용자 `tetris`가 개인키를 읽을 수 있도록 제한된 그룹 권한을 준비한다.
8443은 비특권 포트라 root가 필요 없다. 필요하면 공유기 외부 443을 이 포트로 전달하되,
API 프록시와 같은 IP/포트를 서로 차지하지 않도록 배치를 정한다. 내부 7777·8080은 열지 않는다.

release 스크립트는 WSS를 기본 포함한다(`WSS=0`, Windows `-NoWss`는 로컬 TCP용).
Linux는 libssl·libcrypto를 `lib/`에, macOS는 두 dylib를 `Frameworks/`에 복사하고
참조 경로를 바꾼다. Windows는 빌드 출력의 app-local DLL을 복사하며, 별도 OpenSSL
배포판은 `-TlsRuntimeDir`로 DLL 폴더를 지정한다. 패키지 생성이 깨끗한 OS에서의
실행 검증을 대신하지는 않는다. Linux의 glibc 호환성, macOS 전이 의존성·서명·공증,
Windows VC 런타임과 DLL, 아키텍처는 각각 확인한다.

Windows로 이전할 것은 **동일 버전의 Windows 바이너리·DB 스냅샷·relay secret·
도메인에 맞는 TLS 설정**이다. Linux 실행 파일이나 진행 중 소켓을 복사하지 않는다.
WSS 게이트웨이도 Windows용으로 다시 빌드하고 서비스 시작 순서를 meta → relay →
gateway로 등록한다. systemd 파일은 Windows 서비스 등록 파일이 아니다.
기존 meta의 쓰기를 멈추고 단일 writer만 남기는 전환 절차는
[출시·이전 안내](../release-readiness.md)에 있다.

## 7. 프로토콜 이행과 호환성

바이트 형식은 그대로지만 `[tok_len][token]` 칸의 의미가 장기 계정 토큰에서
게임 입장권으로 바뀐다. 관련 필드 이름을 모두 바꾸지 않은 이유는 기존 프레임·
fixture와의 차이를 작게 유지하기 위해서다. 현재 운영 의미는 이 장을 기준으로 한다.

기본 relay는 장기 토큰을 거절한다. 이전 클라이언트로 돌아가면 ranked 접속이 실패하므로
클라이언트·meta·relay를 같은 릴리스로 배포한다. 테스트 전환용
`TETRIS_RELAY_LEGACY_AUTH=1`만 기존 verify 경로를 명시적으로 허용하며 시작 시 경고한다.
release/systemd 예제는 이 값을 설정하지 않는다. `gt1.` 입장권은 이 호환 모드에서도
항상 한 번만 소비하며 실패 시 캐시로 통과시키지 않는다.

기존 Python 회귀 fixture는 옛 wire 시나리오를 유지하기 위해 호환 모드를 명시한다.
새 `test_secure_admission.py`는 이를 **제거한 기본 보안 모드**로 두 서버를 실행한다.
옛 테스트가 초록이라는 이유만으로 새 보안 경로를 확인했다고 주장하지 않기 위해서다.

## 8. 검증하고 남은 경계를 구분한다

아래 검사는 임시 DB·테스트 전용 인증서·loopback 서버만 사용한다.
`TETRIS_SECURE_BUILD`를 명시했는데 필수 바이너리가 없으면 skip 대신 실패한다.
macOS에서만 구현되지 않은 reactor 조합을 제외하고 portable relay는 검사한다.

```bash
ctest --test-dir build-secure -C Release --output-on-failure
TETRIS_SECURE_BUILD="$PWD/build-secure" TETRIS_META_BIN="$PWD/build-secure/tetris_meta" \
  uv run python -m pytest python/tests/test_secure_admission.py -q
```

Windows multi-config 빌드는 위 환경변수 경로에 `build-secure/Release`를 쓴다.
CI도 Boost·OpenSSL을 준비하고 세 OS에서 WSS 클라이언트 컴파일과 보안 접속 검사를 수행하도록
구성했다. 이 글의 로컬 실행 결과와 아직 실행하지 않은 원격 CI 결과는 구분한다.

| 테스트 | 확인하는 실패·성공 계약 |
|---|---|
| `tests/game_tickets_test.cpp` | 경쟁 소비 1회, 만료 경계, 같은 계정 티켓 교체, 용량 회수 |
| `test_secure_admission.py` | 잘못된 secret, 입장권 목적 제한, 중복 소비, 발급 제한 |
| 같은 파일의 native probe | 정상 인증서, 호스트 불일치·미신뢰 CA·만료 거절, 원격 HTTP 거절 |
| 같은 파일의 Session 경로 | 실제 issuer 콜백 → 방 생성 → 나가기 → 작업 스레드 종료 |
| 같은 파일의 WebSocket 경로 | Origin·경로·text·마스킹·크기, fragmentation·ping/pong, 반복 접속 자원 회수 |
| 같은 파일의 게임 경로 | thread/reactor 각각 랜덤 큐·커스텀 룸·양방향 INPUT 전달 |

최신 수치와 전체 회귀 결과는 [검증 기록](../polish-validation.md)에 모은다.
이 작업에서 해결한 것은 공개 연결의 암호화와 게임 입장 자격의 범위다.
DB 원문 계정 토큰의 해시화·회전·폐기·복구, PvP 결과의 서버 규칙 검증은 후속 작업이다.
봇 BP는 Part 15의 서버 재현 검증을 사용하지만 PvP에도 같은 검증이 생긴 것은 아니다.
브라우저가 접근할 WSS 진입점은 마련했으나 WASM/WebGL 게임·비동기 API·브라우저 저장소는
아직 구현되지 않았다. 따라서 URL을 올리는 것만으로 웹 게임 출시가 끝나지 않는다.
