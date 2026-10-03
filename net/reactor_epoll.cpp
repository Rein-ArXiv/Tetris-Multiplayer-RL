// net/reactor_epoll.cpp — Reactor 의 Linux(epoll) 백엔드
//
// epoll 이 관심 fd 집합을 커널에 보존하고,
// 준비된 것만 epoll_wait 로 돌려준다. 레벨 트리거(기본)를 쓴다 — recv 를 WOULDBLOCK
// 까지 다 비우지 않아도 다음 poll 에서 다시 통지되므로, net/socket.cpp 의
// tcp_recv_some(한 번에 일부만 읽는) 사용 패턴과 그대로 맞물린다.
//
// 깨우기(wake)는 eventfd 로 한다. 다른 스레드나 시그널 직후 종료 플래그를 세운
// 코드가 eventfd 에 8바이트를 쓰면 epoll 대기의 반환 조건이 된다. 실제 재실행은
// OS 스케줄링에 따르며, 일반 스레드 호출 계약과 시그널 처리기 안전성은 구분한다.

#if defined(__linux__)

#include "reactor.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

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

    bool add(NativeSocket fd, unsigned interest, void* token) override {
        return ctl(EPOLL_CTL_ADD, fd, interest, token);
    }
    bool modify(NativeSocket fd, unsigned interest, void* token) override {
        return ctl(EPOLL_CTL_MOD, fd, interest, token);
    }
    bool remove(NativeSocket fd) override {
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
                ssize_t consumed;
                do { consumed = ::read(wakefd_, &sink, sizeof(sink)); }
                while (consumed < 0 && errno == EINTR);
                // 일반 eventfd는 한 번의 성공한 read로 현재 누적값을 비운다.
                // 그 뒤 도착한 wake는 다음 poll에서 처리한다. 계속 쓰는 작업자를
                // 따라 끝없이 drain하면 이 배치의 연결/타이머 처리가 굶을 수 있다.
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

    // 이전 배치 참조와 소유권을 정리한 뒤 다른 인스턴스에 다시 등록할 수 있다.
    bool can_migrate_sockets() const override { return true; }

    void wake() override {
        const int saved_errno = errno;
        uint64_t one = 1;
        ssize_t written;
        do {
            written = ::write(wakefd_, &one, sizeof(one));
        } while (written < 0 && errno == EINTR);
        // EAGAIN means the eventfd already contains a pending notification.
        // Other errors are not proof of a pending wake; the reactor and wakefd
        // must remain alive until all wake callers have stopped.
        errno = saved_errno;
    }

private:
    bool ctl(int op, NativeSocket fd, unsigned interest, void* token) {
        epoll_event ev{};
        ev.events = 0;
        if (interest & kRead)  ev.events |= EPOLLIN | EPOLLRDHUP;
        if (interest & kWrite) ev.events |= EPOLLOUT;
        // EPOLLRDHUP 는 읽기 관심과 함께일 때만 건다. 무조건 걸면 interest 가 0 인
        // 소켓 — 즉 호출자가 백프레셔로 읽기를 멈춰 둔 소켓 — 도 상대가 half-close
        // 하는 순간부터 레벨 트리거로 계속 보고된다. 멈춰 세운 의미가 사라지고
        // 루프가 그 fd 로 스핀할 수 있다. 다만 EPOLLHUP/EPOLLERR는 관심 0에도
        // 보고될 수 있다. interest 0이 모든 종료/오류 관찰을 막는 것은 아니다.
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

#endif // __linux__
