#include "socket.h"
#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>

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

// [NET] 논블로킹 모드 설정
static bool set_nonblocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
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
    // 수락된 소켓을 논블로킹으로 설정
    set_nonblocking(fd);
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
    // 연결된 소켓을 논블로킹으로 설정
    set_nonblocking(fd);
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
        if (n < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                // 논블로킹에서 버퍼 가득참 - 짧은 대기 후 재시도
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            return false;
        }
        if (n == 0) return false; // 연결 종료
#else
        ssize_t n = ::send(s.fd, (const char*)(p + sent), (size_t)(len - sent), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 논블로킹에서 버퍼 가득참 - 짧은 대기 후 재시도
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            return false;
        }
        if (n == 0) return false; // 연결 종료
#endif
        sent += (size_t)n;
    }
    return true;
}

// [NET] 수신 가능한 만큼 한 번 읽어 누적 버퍼에 추가합니다.
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf) {
    uint8_t tmp[4096];
#ifdef _WIN32
    int n = ::recv(s.fd, (char*)tmp, (int)sizeof(tmp), 0);
    if (n < 0) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
            // 논블로킹에서 데이터 없음 - 정상
            return true;
        }
        // 실제 에러
        return false;
    }
    if (n == 0) {
        // 연결 종료
        return false;
    }
#else
    ssize_t n = ::recv(s.fd, tmp, sizeof(tmp), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 논블로킹에서 데이터 없음 - 정상
            return true;
        }
        // 실제 에러
        return false;
    }
    if (n == 0) {
        // 연결 종료
        return false;
    }
#endif
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

std::string get_local_ip() {
    std::string result = "127.0.0.1";  // 기본값

#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints{}, *info = nullptr;
        hints.ai_family = AF_INET;  // IPv4만
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0) {
            struct sockaddr_in* addr = (struct sockaddr_in*)info->ai_addr;
            result = inet_ntoa(addr->sin_addr);
            freeaddrinfo(info);
        }
    }
#else
    // Linux: /proc/net/route를 통해 기본 라우트의 인터페이스를 찾거나
    // 간단히 구글 DNS에 연결해서 로컬 IP 확인
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(53);  // DNS 포트
        inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            socklen_t len = sizeof(addr);
            if (getsockname(sock, (struct sockaddr*)&addr, &len) == 0) {
                result = inet_ntoa(addr.sin_addr);
            }
        }
        close(sock);
    }
#endif

    std::cout << "[NET] Local IP detected: " << result << std::endl;
    return result;
}

std::string get_public_ip() {
    std::string result;

    // 여러 방법 시도 (fallback chain)

    // 방법 1: 빠른 서비스들 시도
    const char* services[] = {
        "https://api.ipify.org",           // 가장 빠름
        "https://ipecho.net/plain",        // 백업
        "https://icanhazip.com",           // 백업
        nullptr
    };

#ifdef _WIN32
    for (const char** service = services; *service != nullptr; ++service) {
        std::string cmd = "powershell -Command \"try { (Invoke-WebRequest -Uri '";
        cmd += *service;
        cmd += "' -UseBasicParsing -TimeoutSec 3).Content } catch { '' }\"";

        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result = buffer;
                // 개행문자 제거
                size_t len = result.length();
                while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r')) {
                    result.pop_back();
                    len--;
                }
                _pclose(pipe);

                // 유효한 IP인지 간단 체크 (숫자와 점만)
                if (!result.empty() && result.find_first_not_of("0123456789.") == std::string::npos) {
                    break;
                }
                result.clear();
            } else {
                _pclose(pipe);
            }
        }
    }
#else
    for (const char** service = services; *service != nullptr; ++service) {
        std::string cmd = "curl -s --connect-timeout 3 ";
        cmd += *service;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result = buffer;
                // 개행문자 제거
                size_t len = result.length();
                while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r')) {
                    result.pop_back();
                    len--;
                }
                pclose(pipe);

                // 유효한 IP인지 간단 체크
                if (!result.empty() && result.find_first_not_of("0123456789.") == std::string::npos) {
                    break;
                }
                result.clear();
            } else {
                pclose(pipe);
            }
        }
    }
#endif

    if (!result.empty()) {
        std::cout << "[NET] Public IP detected: " << result << std::endl;
    } else {
        std::cout << "[NET] Failed to get public IP (check internet connection)" << std::endl;

        // 실패 시 사용자에게 수동 확인 안내
        std::cout << "[NET] Manual check: Visit whatismyip.com in your browser" << std::endl;
    }

    return result;
}

}
