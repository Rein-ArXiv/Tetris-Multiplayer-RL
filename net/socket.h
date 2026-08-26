#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// TCP 소켓 추상화: 플랫폼 독립적 네트워킹 (Windows WinSock / Linux BSD)
// 상세: ARCHITECTURE.md §7.1

namespace net {

// TCP 소켓 핸들 — 참조 카운트 소유(ref-counted owning handle).
//
//   과거에는 평범한 { int fd } 였다. 같은 연결의 복사본을 여러 detached 스레드가
//   값으로 들고 각자 ::close 했기 때문에, 한 스레드가 닫은 fd 정수를 곧바로 새
//   accept() 가 재사용하면 살아있던 다른 스레드가 "엉뚱한 클라이언트 소켓"에
//   read/write 하는 use-after-close / fd-reuse 경합이 있었다(공개 서버에서 교차
//   연결 데이터 유출로 악용 가능).
//
//   이제 fd 는 shared_ptr<int> 가 소유하며, 모든 복사본은 같은 제어 블록을
//   공유한다. 실제 ::close 는 "마지막 복사본이 사라지는 순간" deleter 에서
//   정확히 한 번 호출된다(이중 close 와 fd 재사용 경합 제거).
//
//   tcp_close() 는 즉시 ::shutdown(SHUT_RDWR) 만 호출한다 — 같은 fd 를 폴링/대기
//   중인 다른 복사본의 recv 를 EOF 로 깨워 루프를 빠져나가게 한다. 소유권(=실제
//   close)은 RAII 에 맡긴다. shutdown 은 일반 스레드에서 반복 호출해도 무해한
//   종료 신호로만 사용한다. TcpSocket 은 shared_ptr 를 읽으므로 tcp_close() 를
//   signal handler 에서 직접 호출하면 안 된다.
//
//   동시성 계약: 한 TcpSocket "인스턴스(변수)" 자체를 두 스레드가 동시에
//   재대입/소멸시키면 안 된다(shared_ptr 인스턴스 자체는 thread-safe 가 아님).
//   서로 다른 복사본을 각 스레드가 들고 read/close 하는 것은 안전하다.
struct TcpSocket {
    std::shared_ptr<int> fdh;  // 제어 블록: *fdh == fd. 마지막 참조 소멸 시 ::close.

    int  fd()    const { return fdh ? *fdh : -1; }
    bool valid() const { return fdh && *fdh >= 0; }
};

// 네트워킹 초기화/종료 (Windows: WSAStartup/Cleanup, Linux: no-op)
bool net_init();
void net_shutdown();

// TCP 연결 설정
TcpSocket tcp_listen(uint16_t port, int backlog=1);  // 서버: 포트에서 대기 (bind + listen + SO_REUSEADDR)
// accept 가 실패한 *이유*. 호출자가 EAGAIN 과 fd 고갈을 구분해야 하기 때문이다 —
// 둘 다 "소켓을 못 얻었다" 지만 대응이 정반대다. EAGAIN 은 그냥 다음 이벤트를
// 기다리면 되고, fd 고갈은 기다려도 저절로 낫지 않는다. 레벨 트리거 리스너에서
// 그 둘을 뭉뚱그리면 커널 accept 큐가 안 비어 매 poll 마다 재통지되고, 루프는
// 아무것도 못 하면서 코어를 태운다.
enum class AcceptResult { Ok, WouldBlock, FdExhausted, Error };
// 서버: 클라이언트 연결 수락 (논블로킹 모드로 설정해 반환).
// out_result 를 주면 실패 사유를 받는다. 안 줘도 동작은 같다.
TcpSocket tcp_accept(const TcpSocket& server, AcceptResult* out_result = nullptr);
TcpSocket tcp_connect(const std::string& host, uint16_t port);  // 클라이언트: 서버 연결 (getaddrinfo + connect)

// TCP 데이터 송수신
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len);  // 전체 데이터 송신 (부분 전송 시 재시도)

// 논블로킹 부분 송신. 커널 송신 버퍼가 받아 준 만큼만 보내고 그 길이를 out_sent 에
// 넣는다. 버퍼가 가득 차 한 바이트도 못 보낸 경우(WOULDBLOCK)는 오류가 아니라
// out_sent == 0 으로 나타나며 반환값은 true 다 — 호출자는 남은 바이트를 보류
// 버퍼에 쌓고 쓰기 준비성(Reactor 의 kWrite)을 기다렸다가 다시 부른다.
// 반환 false 는 연결이 끊겼거나 회복 불가 오류라는 뜻이다.
//
// tcp_send_all 과의 차이: 저쪽은 다 보낼 때까지 최대 5초 잠들며 재시도하므로
// 이벤트 루프에서 부르면 그 사이 모든 연결의 전달이 멈춘다. 루프는 이 함수를 쓴다.
bool tcp_send_some(const TcpSocket& s, const void* data, size_t len, size_t& out_sent);
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf);  // 논블로킹 수신 (누적 버퍼에 추가)
void tcp_close(TcpSocket& s);  // shutdown(SHUT_RDWR) 으로 피어/폴러(recv)를 EOF 로 깨운다. 실제 ::close 는 마지막 TcpSocket 복사본 소멸 시 RAII 로 일어난다(멱등).
void tcp_set_nonblocking(const TcpSocket& s);  // 소켓을 논블로킹으로 전환. listen 소켓 accept 폴링용(shutdown 은 블로킹 accept 를 깨우지 못하므로).
void tcp_set_sndbuf(const TcpSocket& s, int bytes);  // 커널 송신 버퍼 상한. 안 읽는 상대를 커널이 대신 흡수하지 못하게 묶는다(backpressure 가시성).
std::string tcp_peer_ip(const TcpSocket& s);   // 로그·밴용 숫자형 peer IP (전체 주소)
// per-IP 상한이 셀 버킷 키. IPv4 는 주소 그대로, IPv6 는 접두사로 묶는다
// (기본 /64 — 가입자 하나가 /64 를 통째로 받는 것이 표준이라 주소 단위로
//  세면 상한이 무력해진다). IPv4-mapped 는 v4 로 되돌려 센다.
std::string tcp_peer_admission_key(const TcpSocket& s, int ipv6_prefix_bits);

// IP 주소 조회
std::string get_local_ip();    // 로컬 네트워크 IP
std::string get_public_ip();   // 공인 IP (ipify.org 사용)

}
