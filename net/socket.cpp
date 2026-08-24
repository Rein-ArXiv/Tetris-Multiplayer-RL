#include "socket.h"
#include <cerrno>
#include <csignal>
#include <cstdio>
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
#  include <mstcpip.h>   // SIO_KEEPALIVE_VALS / tcp_keepalive (set_keepalive 에서 사용)
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

// Lockstep favors latency over batching small INPUT frames.
static int set_nodelay(int fd) {
    int yes = 1;
#ifdef _WIN32
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));
#else
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
#endif
}

// Kernel fallback for peers that disappear without FIN/RST.
static void set_keepalive(int fd) {
    int yes = 1;
#ifdef _WIN32
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&yes, sizeof(yes));
    // Windows 기본 KeepAliveTime 은 2시간이라 SO_KEEPALIVE 만으로는 'FIN/RST 없이
    // 사라진 피어 감지'가 사실상 동작하지 않는다(POSIX 분기의 idle 15s / interval 5s
    // 와 비대칭). SIO_KEEPALIVE_VALS 로 같은 값을 명시해 양 플랫폼 감지 시간을 맞춘다.
    // (Vista+ 는 probe 재전송 횟수가 10회 고정 — 대략 15s + 10*5s 내 감지.)
    tcp_keepalive ka{};
    ka.onoff = 1;
    ka.keepalivetime = 15000;     // idle 15초 후 첫 probe (ms)
    ka.keepaliveinterval = 5000;  // probe 간격 5초 (ms)
    DWORD bytesReturned = 0;
    // keepalive 는 best-effort 폴백이라 setsockopt/WSAIoctl 실패는 조용히 무시한다
    // (실패해도 연결 자체는 정상 동작하고, 상위의 PING/PONG 타임아웃이 최후 방어선).
    WSAIoctl((SOCKET)fd, SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
             nullptr, 0, &bytesReturned, nullptr, nullptr);
#else
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#  if defined(TCP_KEEPIDLE)
    int idle = 15;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#  elif defined(TCP_KEEPALIVE)
    int idle = 15;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
#  endif
#  if defined(TCP_KEEPINTVL)
    int interval = 5;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
#  endif
#  if defined(TCP_KEEPCNT)
    int probes = 3;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes));
#  endif
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
    set_keepalive(fd);
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
    set_keepalive(fd);
    return make_owned(fd);
}

std::string tcp_peer_ip(const TcpSocket& s) {
    if (!s.valid()) return {};
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    if (::getpeername(s.fd(), reinterpret_cast<sockaddr*>(&addr), &len) != 0) return {};
    char host[NI_MAXHOST]{};
    if (::getnameinfo(reinterpret_cast<sockaddr*>(&addr), len,
                      host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0) return {};
    return host;
}

// [NET] 전체 버퍼가 전송될 때까지 반복합니다(스트림 특성으로 부분 전송 가능).
// 논블로킹 부분 송신 — 이벤트 루프용. 계약은 net/socket.h 참조.
bool tcp_send_some(const TcpSocket& s, const void* data, size_t len, size_t& out_sent) {
    out_sent = 0;
    const int fd = s.fd();
    if (fd < 0) return false;
    if (len == 0) return true;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(fd, (const char*)(p + sent), (int)(len - sent), 0);
        if (n < 0) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
            // 버퍼 가득참 — 오류가 아니다. 보낸 만큼만 보고하고 돌아간다.
            if (err == WSAEWOULDBLOCK) break;
            return false;
        }
        if (n == 0) return false;  // 연결 종료
#else
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        ssize_t n = ::send(fd, (const char*)(p + sent), (size_t)(len - sent), flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;
        }
        if (n == 0) return false;  // 연결 종료
#endif
        sent += (size_t)n;
    }
    out_sent = sent;
    return true;
}

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

// shutdown wakes peer threads; the final handle owner closes the fd.
// 불변식: signal handler 에서 tcp_close() 호출 금지 — shared_ptr(fdh) 읽기는 async-signal-safe 가 아니다.
// 불변식: 여기서 fdh.reset() 금지 — 같은 인스턴스를 읽는 다른 스레드와 shared_ptr
//         인스턴스 경합이 된다. 참조 해제는 소유 스레드의 RAII(재대입/소멸)에 맡긴다.
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

// [NET] 커널 송신 버퍼 상한을 지정한다.
//   기본값을 두면 Linux 는 안 읽는 상대에게 보내는 바이트를 소켓당 tcp_wmem
//   최대(≈4MiB)까지 커널이 조용히 흡수한다 — 유저스페이스 송신 큐가 안 쌓이므로
//   그 위에 지은 backpressure/예산은 커널 몫만큼 늦게 걸린다. 상한을 걸면
//   밀림이 수 초 안에 유저스페이스로 드러난다.
//   주의: Linux 는 관리 오버헤드 몫으로 요청값의 2배를 잡는다 (64KiB 요청 ⇒ 실효 128KiB).
//   실패는 best-effort 로 무시한다 — 상한을 못 걸어도 연결 자체는 정상 동작한다.
void tcp_set_sndbuf(const TcpSocket& s, int bytes) {
    if (!s.valid() || bytes <= 0) return;
#ifdef _WIN32
    setsockopt(s.fd(), SOL_SOCKET, SO_SNDBUF, (const char*)&bytes, sizeof(bytes));
#else
    setsockopt(s.fd(), SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
#endif
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
            char ipbuf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr->sin_addr, ipbuf, sizeof(ipbuf))) {
                result = ipbuf;
            }
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
                char ipbuf[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf))) {
                    result = ipbuf;
                }
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
