#if defined(__linux__)
#include "net/epoll_reactor.h"
#include "net/unique_fd.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <array>
#include <cerrno>
namespace study_net {
namespace {
class EpollReactor final : public StudyReactor {
 public:
    bool init() {
        epoll_.reset(::epoll_create1(EPOLL_CLOEXEC));
        if (!epoll_.valid()) return false;
        wake_.reset(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
        if (!wake_.valid()) return false;
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.u64 = 0; // Registration IDs start at 1; 0 belongs to wake.
        return ::epoll_ctl(epoll_.get(), EPOLL_CTL_ADD, wake_.get(), &event) == 0;
    }
    std::optional<Registration> watch(const Socket& socket, unsigned interest) override {
        const auto id = registrations_.add(socket.native(), interest);
        if (!id) return std::nullopt;
        if (interest != 0 && !control(EPOLL_CTL_ADD, socket.native(), *id, interest)) {
            registrations_.remove(*id); // Roll back membership, never reuse the spent ID.
            return std::nullopt;
        }
        return id;
    }
    bool change(Registration id, unsigned interest) override {
        const auto* entry = registrations_.find(id);
        if (!entry || (interest & ~kInterestMask) != 0) return false;
        const auto old = entry->interest;
        if (old == interest) return true;
        // A paused registration stays in our table, but not in epoll's interest list.
        const int operation = old == 0 ? EPOLL_CTL_ADD :
                              interest == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
        if (!control(operation, entry->fd, id, interest)) return false;
        return registrations_.modify(id, interest); // Non-allocating commit after OS success.
    }
    bool unwatch(Registration id) override {
        const auto* entry = registrations_.find(id);
        if (!entry) return false;
        if (entry->interest != 0 && !control(EPOLL_CTL_DEL, entry->fd, id, 0)) return false;
        return registrations_.remove(id);
    }
    bool current(Registration id) const override { return registrations_.find(id) != nullptr; }
    bool wake() noexcept override {
        const int saved_errno = errno;
        const std::uint64_t one = 1;
        ssize_t sent;
        do { sent = ::write(wake_.get(), &one, sizeof(one)); }
        while (sent < 0 && errno == EINTR);
        const bool notified = sent == sizeof(one) || (sent < 0 && errno == EAGAIN);
        errno = saved_errno;
        return notified;
    }
    ReadyBatch poll(int timeout_ms) override {
        ReadyBatch result;
        result.events.reserve(Registrations::capacity);
        if (timeout_ms < 0 || timeout_ms > 1000) {
            result.state = PollState::error; result.error = EINVAL; return result;
        }
        const int count = ::epoll_wait(epoll_.get(), scratch_.data(),
                                      static_cast<int>(scratch_.size()), timeout_ms);
        if (count == 0) return result;
        if (count < 0) {
            result.error = errno;
            result.state = result.error == EINTR ? PollState::interrupted : PollState::error;
            return result;
        }
        result.state = PollState::events;
        for (int i = 0; i < count; ++i) {
            const auto& event = scratch_[i];
            const Registration id = event.data.u64;
            if (id == 0) {
                result.woken = true;
                std::uint64_t pending;
                ssize_t read;
                do { read = ::read(wake_.get(), &pending, sizeof(pending)); }
                while (read < 0 && errno == EINTR);
                // One successful read absorbs the current counter. Concurrent writers
                // may leave a new notification for the next poll; do not drain forever.
                if (read != sizeof(pending) && !(read < 0 && errno == EAGAIN)) {
                    result.state = PollState::error; result.error = read < 0 ? errno : EIO;
                    return result;
                }
                continue;
            }
            const auto* entry = registrations_.find(id);
            if (!entry || entry->interest == 0) continue;
            const bool fault = (event.events & (EPOLLERR | EPOLLHUP)) != 0;
            result.events.push_back({id, (event.events & (EPOLLIN | EPOLLRDHUP)) != 0 || fault,
                                     (event.events & EPOLLOUT) != 0, fault});
        }
        return result; // Removal after return still requires dispatch-time validation.
    }
 private:
    bool control(int operation, int fd, Registration id, unsigned interest) {
        epoll_event event{};
        if (interest & Read) event.events |= EPOLLIN | EPOLLRDHUP;
        if (interest & Write) event.events |= EPOLLOUT;
        event.data.u64 = id;
        return ::epoll_ctl(epoll_.get(), operation, fd, &event) == 0;
    }
    UniqueFd epoll_, wake_;
    Registrations registrations_;
    std::array<epoll_event, Registrations::capacity + 1> scratch_{};
};
} // namespace
std::unique_ptr<StudyReactor> make_epoll_reactor() {
    auto reactor = std::make_unique<EpollReactor>();
    if (!reactor->init()) return nullptr;
    return reactor;
}
} // namespace study_net
#endif
