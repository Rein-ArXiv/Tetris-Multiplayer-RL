#include "socket.h"
#include <cstring>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX 1
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "ws2_32.lib")
#  endif
using socklen_t = int;
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
#endif

namespace net {

static bool g_inited = false;

// [NET] 네트워킹 초기화(Windows 전용)
bool net_init() {
    if (g_inited) return true;
#ifdef _WIN32
    WSADATA wsaData;
    int r = WSAStartup(MAKEWORD(2,2), &wsaData);
    g_inited = (r == 0);
    return g_inited;
#else
    g_inited = true;
    return true;
#endif
}

// [NET] 네트워킹 정리(Windows 전용)
void net_shutdown() {
    if (!g_inited) return;
#ifdef _WIN32
    WSACleanup();
#endif
    g_inited = false;
}

// [NET] 빠른 재바인드를 위한 SO_REUSEADDR 설정
static int set_reuse(int fd) {
    int yes = 1;
#ifdef _WIN32
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#else
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
}

// [NET] 포트에서 연결 대기 소켓을 생성합니다.
TcpSocket tcp_listen(uint16_t port, int backlog) {
    TcpSocket s{};
    if (!net_init()) return s;
    int fd = (int)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return s;
    set_reuse(fd);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return s;
    }
    if (::listen(fd, backlog) != 0) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return s;
    }
    s.fd = fd;
    return s;
}

// [NET] 대기 소켓에서 1개 연결을 수락합니다.
TcpSocket tcp_accept(const TcpSocket& server) {
    TcpSocket c{};
    if (!server.valid()) return c;
    sockaddr_in addr{}; socklen_t alen = sizeof(addr);
    int fd = (int)::accept(server.fd, (sockaddr*)&addr, &alen);
    if (fd < 0) return c;
    c.fd = fd;
    return c;
}

// [NET] 원격 호스트로 TCP 연결을 시도합니다.
TcpSocket tcp_connect(const std::string& host, uint16_t port) {
    TcpSocket s{};
    if (!net_init()) return s;

    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr; char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0) return s;
    int fd = -1;
    for (addrinfo* p = res; p; p = p->ai_next) {
        fd = (int)::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break;
        }
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return s;
    s.fd = fd;
    return s;
}

// [NET] 전체 버퍼가 전송될 때까지 반복합니다(스트림 특성으로 부분 전송 가능).
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(s.fd, (const char*)(p + sent), (int)(len - sent), 0);
#else
        ssize_t n = ::send(s.fd, (const char*)(p + sent), (size_t)(len - sent), 0);
#endif
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

// [NET] 수신 가능한 만큼 한 번 읽어 누적 버퍼에 추가합니다.
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf) {
    uint8_t tmp[4096];
#ifdef _WIN32
    int n = ::recv(s.fd, (char*)tmp, (int)sizeof(tmp), 0);
#else
    ssize_t n = ::recv(s.fd, tmp, sizeof(tmp), 0);
#endif
    if (n <= 0) return false;
    outBuf.insert(outBuf.end(), tmp, tmp + n);
    return true;
}

// [NET] 소켓 종료
void tcp_close(TcpSocket& s) {
    if (!s.valid()) return;
#ifdef _WIN32
    closesocket(s.fd);
#else
    close(s.fd);
#endif
    s.fd = -1;
}

}
