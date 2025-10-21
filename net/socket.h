#pragma once
#include <cstdint>
#include <string>
#include <vector>

// TCP 소켓 추상화: 플랫폼 독립적 네트워킹 (Windows WinSock / Linux BSD)
// 상세: DOCUMENTATION.md

namespace net {

// TCP 소켓 래퍼 (fd: 파일 디스크립터, -1 = 유효하지 않음)
struct TcpSocket {
    int fd{-1};
    bool valid() const { return fd >= 0; }
};

// 네트워킹 초기화/종료 (Windows: WSAStartup/Cleanup, Linux: no-op)
bool net_init();
void net_shutdown();

// TCP 연결 설정
TcpSocket tcp_listen(uint16_t port, int backlog=1);  // 서버: 포트에서 대기 (bind + listen + SO_REUSEADDR)
TcpSocket tcp_accept(const TcpSocket& server);       // 서버: 클라이언트 연결 수락 (블로킹, 논블로킹 모드로 설정)
TcpSocket tcp_connect(const std::string& host, uint16_t port);  // 클라이언트: 서버 연결 (getaddrinfo + connect)

// TCP 데이터 송수신
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len);  // 전체 데이터 송신 (부분 전송 시 재시도)
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf);  // 논블로킹 수신 (누적 버퍼에 추가)
void tcp_close(TcpSocket& s);  // 소켓 종료 (4-way handshake)

// IP 주소 조회
std::string get_local_ip();    // 로컬 네트워크 IP
std::string get_public_ip();   // 공인 IP (ipify.org 사용)

}
