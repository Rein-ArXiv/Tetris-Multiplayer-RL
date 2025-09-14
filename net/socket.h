#pragma once
#include <cstdint>
#include <string>
#include <vector>

// [NET] 최소 TCP 래퍼. Windows(WinSock2)와 Linux(BSD 소켓)를 #ifdef로 분기합니다.
// 학습 포인트: 연결 지향/스트림, 논블로킹 vs 블로킹, send/recv의 부분 전송 처리.

namespace net {

struct TcpSocket {
    int fd{-1};
    bool valid() const { return fd >= 0; }
};

bool net_init();           // [NET] Windows에서 WSAStartup, Linux는 무동작
void net_shutdown();       // [NET] Windows에서 WSACleanup

TcpSocket tcp_listen(uint16_t port, int backlog=1);
TcpSocket tcp_accept(const TcpSocket& server);
TcpSocket tcp_connect(const std::string& host, uint16_t port);

bool tcp_send_all(const TcpSocket& s, const void* data, size_t len);
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf); // [NET] 가변 길이 수신 버퍼(프레이밍에 넘김)
void tcp_close(TcpSocket& s);

}

