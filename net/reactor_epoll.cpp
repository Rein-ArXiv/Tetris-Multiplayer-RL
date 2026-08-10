// net/reactor_epoll.cpp — Reactor 의 Linux(epoll) 백엔드
//
// 준비성 모델의 원산지라 구현이 가장 곧다. epoll 이 관심 fd 집합을 커널에 두고,
// 준비된 것만 epoll_wait 로 돌려준다. 레벨 트리거(기본)를 쓴다 — recv 를 WOULDBLOCK
// 까지 다 비우지 않아도 다음 poll 에서 다시 통지되므로, net/socket.cpp 의
// tcp_recv_some(한 번에 일부만 읽는) 사용 패턴과 그대로 맞물린다.
//
// 깨우기(wake)는 eventfd 로 한다. 다른 스레드나 시그널 직후 종료 플래그를 세운
// 코드가 eventfd 에 8바이트를 쓰면, epoll 이 그것을 준비 이벤트로 보고 poll 이
// 즉시 반환한다.

#if !defined(_WIN32)

#include "reactor.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>

namespace net {

namespace {

class EpollReactor final : public Reactor {
public:
    bool init() {
        epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epfd_ < 0) return false;
        // 논블로킹 + close-on-exec eventfd. 값 누적 방식이라 여러 wake 가 몰려도
        // 한 번의 read 로 흡수된다.
        wakefd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (wakefd_ < 0) { ::close(epfd_); epfd_ = -1; return false; }
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.ptr = &wake_marker_;  // 연결 token 과 겹치지 않는 고유 표식
        if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, wakefd_, &ev) != 0) return false;
        return true;
    }

    ~EpollReactor() override {
        if (wakefd_ >= 0) ::close(wakefd_);
        if (epfd_   >= 0) ::close(epfd_);
    }

    bool add(int fd, unsigned interest, void* token) override {
        return ctl(EPOLL_CTL_ADD, fd, interest, token);
    }
    bool modify(int fd, unsigned interest, void* token) override {
        return ctl(EPOLL_CTL_MOD, fd, interest, token);
    }
    bool remove(int fd) override {
        // Linux 2.6.9+ 는 마지막 인자가 무시되지만, 널을 넘기지 않도록 더미를 준다.
        epoll_event ev{};
        return ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev) == 0;
    }

    int poll(std::vector<Event>& out, int timeout_ms) override {
        out.clear();
        if (scratch_.empty()) scratch_.resize(256);
        int n = ::epoll_wait(epfd_, scratch_.data(),
                             static_cast<int>(scratch_.size()), timeout_ms);
        if (n < 0) {
            if (errno == EINTR) return 0;  // 시그널 — 만기 처리하러 나간다
            return -1;
        }
        for (int i = 0; i < n; ++i) {
            const epoll_event& e = scratch_[i];
            if (e.data.ptr == &wake_marker_) {
                uint64_t sink;
                while (::read(wakefd_, &sink, sizeof(sink)) > 0) {}  // 값 흡수
                continue;  // wake 는 이벤트로 노출하지 않는다
            }
            Event out_ev;
            out_ev.token    = e.data.ptr;
            out_ev.readable  = (e.events & (EPOLLIN | EPOLLRDHUP)) != 0;
            out_ev.writable  = (e.events & EPOLLOUT) != 0;
            out_ev.error     = (e.events & (EPOLLERR | EPOLLHUP)) != 0;
            // 오류는 다음 recv 가 확정 처리하도록 readable 로도 표시한다.
            if (out_ev.error) out_ev.readable = true;
            out.push_back(out_ev);
        }
        return static_cast<int>(out.size());
    }

    // epoll 인스턴스에서 빼고 다른 인스턴스에 다시 넣으면 그만이다.
    bool can_migrate_sockets() const override { return true; }

    void wake() override {
        uint64_t one = 1;
        // 논블로킹 write 실패(버퍼 포화)는 이미 미소비 wake 가 대기 중이라는 뜻이라
        // 무시해도 안전하다.
        ssize_t r = ::write(wakefd_, &one, sizeof(one));
        (void)r;
    }

private:
    bool ctl(int op, int fd, unsigned interest, void* token) {
        epoll_event ev{};
        ev.events = EPOLLRDHUP;
        if (interest & kRead)  ev.events |= EPOLLIN;
        if (interest & kWrite) ev.events |= EPOLLOUT;
        ev.data.ptr = token;
        return ::epoll_ctl(epfd_, op, fd, &ev) == 0;
    }

    int epfd_   = -1;
    int wakefd_ = -1;
    char wake_marker_ = 0;
    std::vector<epoll_event> scratch_;
};

} // namespace

std::unique_ptr<Reactor> Reactor::create() {
    auto r = std::unique_ptr<EpollReactor>(new EpollReactor());
    if (!r->init()) return nullptr;
    return r;
}

} // namespace net

#endif // !_WIN32
