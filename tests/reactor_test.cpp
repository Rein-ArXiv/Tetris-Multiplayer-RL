// tests/reactor_test.cpp — net::Reactor 백엔드 회귀 테스트
//
// 서버 루프를 이관하기 전에 reactor 자체의 계약을 격리해 검증한다. 루프백으로
// 연결된 소켓 한 쌍을 reactor 에 등록하고, 준비성 통지 → recv → send 왕복이
// 실제로 도는지, 그리고 wake() 가 다른 스레드에서 poll 을 깨우는지 확인한다.
//
// 플랫폼별로 다른 백엔드를 검증한다: Windows = IOCP(zero-byte WSARecv 준비성
// 에뮬레이션), Linux = epoll. 프로토콜과 무관한 순수 전송 계층 테스트다.

#include "../net/reactor.h"
#include "../net/socket.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  using socklen_t = int;  // winsock 은 주소 길이에 int 를 쓴다
#else
  #include <netinet/in.h>
  #include <sys/socket.h>
#endif

namespace {

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "[reactor-test] FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::fprintf(stderr, "[reactor-test] ok:   %s\n", what);
    }
}

// listen 소켓이 실제로 바인딩된 포트를 읽는다(ephemeral 포트 0 사용 시 필요).
uint16_t bound_port(int fd) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(static_cast<
#if defined(_WIN32)
            SOCKET
#else
            int
#endif
            >(fd), reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return 0;
    }
    return ntohs(addr.sin_port);
}

// out 에 원하는 token 의 readable 이벤트가 올 때까지 최대 deadline_ms 동안 돈다.
bool wait_readable(net::Reactor& r, void* want, int deadline_ms) {
    std::vector<net::Event> evs;
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int n = r.poll(evs, 200);
        if (n < 0) return false;
        for (const auto& e : evs) {
            if (e.token == want && e.readable) return true;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > deadline_ms) return false;
    }
}

// 논블로킹 소켓에서 정확히 want 바이트가 모일 때까지 reactor 준비성에 맞춰 읽는다.
std::string recv_exact(net::Reactor& r, const net::TcpSocket& s, void* tok,
                       size_t want) {
    std::vector<uint8_t> buf;
    while (buf.size() < want) {
        if (!wait_readable(r, tok, 3000)) break;
        net::tcp_recv_some(s, buf);
    }
    return std::string(buf.begin(), buf.end());
}

} // namespace

int main() {
    if (!net::net_init()) {
        std::fprintf(stderr, "[reactor-test] net_init failed\n");
        return 2;
    }

    auto reactor = net::Reactor::create();
    check(reactor != nullptr, "Reactor::create()");
    if (!reactor) { net::net_shutdown(); return 2; }

    // 루프백 연결 한 쌍을 만든다: listen → 백그라운드 connect → accept.
    net::TcpSocket listener = net::tcp_listen(0, 1);
    check(listener.valid(), "tcp_listen(ephemeral)");
    uint16_t port = bound_port(listener.fd());
    check(port != 0, "bound_port");

    net::TcpSocket client;
    std::thread connector([&] {
        client = net::tcp_connect("127.0.0.1", port);
    });
    net::TcpSocket server = net::tcp_accept(listener);
    connector.join();
    check(server.valid(), "tcp_accept");
    check(client.valid(), "tcp_connect");

    // accept/connect 가 만든 소켓은 논블로킹으로 설정돼 있다(net/socket.cpp 계약).
    int client_tag = 1, server_tag = 2;
    check(reactor->add(client.fd(), net::kRead, &client_tag), "add(client)");
    check(reactor->add(server.fd(), net::kRead, &server_tag), "add(server)");

    // 왕복 1: client --"ping"--> server
    check(net::tcp_send_all(client, "ping", 4), "send ping");
    std::string got = recv_exact(*reactor, server, &server_tag, 4);
    check(got == "ping", "server received 'ping'");

    // 왕복 2: server --"pong"--> client (준비성이 반대 방향으로도 동작하는지)
    check(net::tcp_send_all(server, "pong", 4), "send pong");
    std::string got2 = recv_exact(*reactor, client, &client_tag, 4);
    check(got2 == "pong", "client received 'pong'");

    // wake(): 다른 스레드에서 블로킹 중인 poll 을 깨운다.
    std::atomic<bool> woke{false};
    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        reactor->wake();
    });
    auto t0 = std::chrono::steady_clock::now();
    std::vector<net::Event> evs;
    reactor->poll(evs, 5000);  // wake 없으면 5초 블로킹, wake 로 곧 반환돼야 함
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    woke = (ms < 2000);
    waker.join();
    check(woke.load(), "wake() unblocked poll within 2s");

    // 무장된 상태에서의 remove — 연결 정리 경로가 실제로 쓰는 순서다.
    // IOCP 백엔드는 커널이 아직 OVERLAPPED 를 들고 있으므로 상태 객체를 바로
    // 해제하면 뒤늦은 완료 통지가 해제된 메모리를 가리킨다. 취소 완료를 회수할
    // 때까지 살려 두는지 확인한다(여기서 죽거나 제거된 token 이 다시 나오면 실패).
    check(reactor->remove(client.fd()), "remove(client) while read-armed");
    net::tcp_close(client);
    std::vector<net::Event> after;
    bool leaked_removed_token = false;
    for (int i = 0; i < 3; ++i) {
        reactor->poll(after, 50);
        for (const auto& e : after) {
            if (e.token == &client_tag) leaked_removed_token = true;
        }
    }
    check(!leaked_removed_token, "removed token no longer reported");

    check(reactor->remove(server.fd()), "remove(server)");
    net::tcp_close(server);
    net::tcp_close(listener);
    net::net_shutdown();

    if (g_failures == 0) {
        std::fprintf(stderr, "[reactor-test] all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "[reactor-test] %d check(s) failed\n", g_failures);
    return 1;
}
