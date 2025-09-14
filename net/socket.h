#pragma once
#include <cstdint>
#include <string>
#include <vector>

// [NET] 최소 TCP 래퍼
// - 운영체제 소켓 API(Windows: WinSock2, Linux: BSD 소켓)를 얇게 감싼 인터페이스입니다.
// - TCP는 연결 지향 스트림이므로 메시지 경계가 없고, send/recv가 부분 전송될 수 있습니다.
// - 이 레이어는 "바이트 스트림"만 책임지고, 메시지 경계/무결성은 framing에서 다룹니다.

namespace net {

struct TcpSocket {
    int fd{-1};
    bool valid() const { return fd >= 0; }
};

// [NET] 초기화/정리
// - Windows: WSAStartup/WSACleanup
// - Linux: 무동작
bool net_init();
void net_shutdown();

// [NET] 접속/수락
// - tcp_listen: 지정 포트에서 대기 소켓 생성
// - tcp_accept: 대기 소켓에서 1개 연결 수락
// - tcp_connect: 원격 {host,port}로 연결 시도
TcpSocket tcp_listen(uint16_t port, int backlog=1);
TcpSocket tcp_accept(const TcpSocket& server);
TcpSocket tcp_connect(const std::string& host, uint16_t port);

// [NET] 송수신
// - tcp_send_all: 버퍼 전체가 전송될 때까지 반복 호출(블로킹)
// - tcp_recv_some: 수신 가능한 만큼 한 번 읽어 누적 버퍼에 추가
//   framing 단계에서 누적 버퍼를 파싱해 메시지 경계를 복원합니다.
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len);
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf);
void tcp_close(TcpSocket& s);

}
