#include "socket.h"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
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
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
#endif

namespace net {

static bool g_inited = false;

// [NET] 실제 fd 를 닫는다(플랫폼별). 오직 owning 핸들의 deleter 에서만 호출.
static void close_fd(int fd) {
    if (fd < 0) return;
#ifdef _WIN32
    closesocket(fd);
#else
    ::close(fd);
#endif
}

// [NET] 새로 생성된 fd 를 참조 카운트 소유 핸들로 감싼다.
//   마지막 복사본이 사라질 때 deleter 가 close_fd 로 정확히 한 번 닫는다.
static TcpSocket make_owned(int fd) {
    TcpSocket s;
    s.fdh = std::shared_ptr<int>(new int(fd), [](int* p) {
        if (p) { close_fd(*p); delete p; }
    });
    return s;
}

// [NET] 네트워킹 초기화(Windows 전용)
bool net_init() {
    if (g_inited)
        return true;
#ifdef _WIN32
    WSADATA wsaData;
    int r = WSAStartup(MAKEWORD(2,2), &wsaData);
    g_inited = (r == 0);
    return g_inited;
#else
    // POSIX: writing to a closed peer can raise SIGPIPE and terminate the whole
    // relay/client process before send() returns EPIPE. Treat it as an I/O error.
    std::signal(SIGPIPE, SIG_IGN);
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

// [NET] Nagle 비활성화 (TCP_NODELAY).
//   기본 Nagle 알고리즘은 작은 패킷(<MSS) 을 최대 200ms 까지 버퍼링해 모아
//   보낸다. 우리 INPUT 프레임은 7바이트 / 60Hz 로 송신 → Nagle ON 이면 각
//   프레임이 수십~200ms 지연되어 도착한다. lockstep 의 safeTick 은 상대 INPUT
//   도착까지 대기하므로 → 체감상 "호스트가 렉 걸림".
//   게임 트래픽은 지연이 대역폭보다 압도적으로 치명적 → 반드시 NODELAY.
static int set_nodelay(int fd) {
    int yes = 1;
#ifdef _WIN32
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));
#else
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
#endif
}

// [NET] 포트에서 연결 대기 소켓을 생성합니다.
TcpSocket tcp_listen(uint16_t port, int backlog) {
    if (!net_init()) return TcpSocket{};
    int fd = (int)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return TcpSocket{};
    set_reuse(fd);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
        close_fd(fd);
        return TcpSocket{};
    }
    if (::listen(fd, backlog) != 0) {
        close_fd(fd);
        return TcpSocket{};
    }
    return make_owned(fd);
}

// [NET] 대기 소켓에서 1개 연결을 수락합니다.
TcpSocket tcp_accept(const TcpSocket& server) {
    if (!server.valid()) return TcpSocket{};
    sockaddr_in addr{}; socklen_t alen = sizeof(addr);
    int fd = (int)::accept(server.fd(), (sockaddr*)&addr, &alen);
    if (fd < 0) return TcpSocket{};
    // 수락된 소켓을 논블로킹 + NODELAY 로 설정.
    set_nonblocking(fd);
    set_nodelay(fd);
    return make_owned(fd);
}

// [NET] 원격 호스트로 TCP 연결을 시도합니다.
TcpSocket tcp_connect(const std::string& host, uint16_t port) {
    if (!net_init()) return TcpSocket{};

    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr; char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0) return TcpSocket{};
    int fd = -1;
    for (addrinfo* p = res; p; p = p->ai_next) {
        fd = (int)::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break;
        }
        close_fd(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return TcpSocket{};
    // 연결된 소켓을 논블로킹 + NODELAY 로 설정.
    set_nonblocking(fd);
    set_nodelay(fd);
    return make_owned(fd);
}

// [NET] 전체 버퍼가 전송될 때까지 반복합니다(스트림 특성으로 부분 전송 가능).
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len) {
    const int fd = s.fd();
    if (fd < 0) return false;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    constexpr auto kBlockedTimeout = std::chrono::seconds(5);
    std::chrono::steady_clock::time_point blockedSince{};
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(fd, (const char*)(p + sent), (int)(len - sent), 0);
        if (n < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) {
                if (err == WSAEWOULDBLOCK) {
                    auto now = std::chrono::steady_clock::now();
                    if (blockedSince == std::chrono::steady_clock::time_point{}) blockedSince = now;
                    if (now - blockedSince >= kBlockedTimeout) return false;
                }
                // 논블로킹에서 버퍼 가득참 - 짧은 대기 후 재시도
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            return false;
        }
        if (n == 0) return false; // 연결 종료
#else
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        ssize_t n = ::send(fd, (const char*)(p + sent), (size_t)(len - sent), flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                auto now = std::chrono::steady_clock::now();
                if (blockedSince == std::chrono::steady_clock::time_point{}) blockedSince = now;
                if (now - blockedSince >= kBlockedTimeout) return false;
                // 논블로킹에서 버퍼 가득참 - 짧은 대기 후 재시도
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            return false;
        }
        if (n == 0) return false; // 연결 종료
#endif
        blockedSince = {};
        sent += (size_t)n;
    }
    return true;
}

// [NET] 수신 가능한 만큼 한 번 읽어 누적 버퍼에 추가합니다.
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf) {
    const int fd = s.fd();
    if (fd < 0) return false;
    uint8_t tmp[4096];
#ifdef _WIN32
    int n = ::recv(fd, (char*)tmp, (int)sizeof(tmp), 0);
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
    ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
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

// [NET] 소켓 종료.
//   ::shutdown 으로 같은 fd 를 폴링/대기 중인 다른 복사본의 recv 를 EOF 로
//   깨워 루프를 빠져나가게 한다. 실제 ::close 는 마지막 TcpSocket 복사본이
//   소멸할 때 deleter 에서 한 번만 일어난다(이중 close / fd 재사용 경합 방지).
//   shutdown 은 일반 스레드에서 반복 호출해도 무해한 종료 신호로만 사용한다.
//   TcpSocket 은 shared_ptr 를 읽으므로 tcp_close() 를 signal handler 에서 직접
//   호출하면 안 된다.
//   여기서 fdh 를 reset 하지 않는 이유: 같은 인스턴스를 다른 스레드가 읽고 있을
//   수 있어(예: Session::sock 을 ioThread 가 read, 메인이 Close) reset 은
//   shared_ptr 인스턴스에 대한 경합이 된다. 참조 해제는 RAII(소유 스레드의
//   재대입/소멸)에 맡긴다.
void tcp_close(TcpSocket& s) {
    if (!s.fdh) return;
    int fd = *s.fdh;
    if (fd >= 0) {
#ifdef _WIN32
        ::shutdown(fd, SD_BOTH);
#else
        ::shutdown(fd, SHUT_RDWR);
#endif
    }
}

// [NET] 소켓을 논블로킹 모드로 전환(public 래퍼).
void tcp_set_nonblocking(const TcpSocket& s) {
    if (s.valid()) set_nonblocking(s.fd());
}

std::string get_local_ip() {
    std::string result = "127.0.0.1";  // 기본값

#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints{}, *info = nullptr;
        hints.ai_family = AF_INET;  // IPv4만
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0 && info != nullptr) {
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
