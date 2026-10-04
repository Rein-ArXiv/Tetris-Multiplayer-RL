#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <poll.h>
#include <cerrno>
#endif
#include "net/study_reactor.h"
#include "net/send_socket.h"
#include "net/receive_socket.h"
#include <array>
#include <stdexcept>
namespace study_net {
namespace {
#ifdef _WIN32
using PollFd = WSAPOLLFD;
constexpr short read_bit = POLLRDNORM, write_bit = POLLWRNORM;
int invalid_error() { return WSAEINVAL; }
#else
using PollFd = pollfd;
constexpr short read_bit = POLLIN, write_bit = POLLOUT;
int invalid_error() { return EINVAL; }
#endif
class PollReactor final : public StudyReactor {
 public:
    PollReactor() {
        int error = 0;
        std::uint16_t port = 0;
        auto listener = listen_loopback(0, port, error);
        if (!listener.valid()) throw std::runtime_error("wake listen");
        wake_send_ = connect_loopback(port, error);
        if (!wake_send_.valid()) throw std::runtime_error("wake connect");
        wake_read_ = accept_one(listener, error);
        if (!wake_read_.valid() || !set_nonblocking(wake_send_, true, error) ||
            !set_nonblocking(wake_read_, true, error)) throw std::runtime_error("wake setup");
    }
    std::optional<Registration> watch(const Socket& socket, unsigned interest) override {
        return registrations_.add(socket.native(), interest);
    }
    bool change(Registration id, unsigned interest) override { return registrations_.modify(id, interest); }
    bool unwatch(Registration id) override { return registrations_.remove(id); }
    bool current(Registration id) const override { return registrations_.find(id) != nullptr; }
    bool wake() noexcept override {
        const std::uint8_t marker = 1;
        for (;;) {
            const auto sent = try_send(wake_send_, &marker, 1);
            if (sent.state == SendState::interrupted) continue;
            // Full socket buffer means a notification is already queued for this loop.
            return sent.state == SendState::progress || sent.state == SendState::would_block;
        }
    }
    ReadyBatch poll(int timeout_ms) override {
        ReadyBatch result;
        result.events.reserve(Registrations::capacity);
        if (timeout_ms < 0 || timeout_ms > 1000) {
            result.state = PollState::error; result.error = invalid_error(); return result;
        }
        std::array<PollFd, Registrations::capacity + 1> fds{};
        std::array<Registration, Registrations::capacity + 1> ids{};
        fds[0].fd = wake_read_.native(); fds[0].events = read_bit;
        std::size_t count = 1;
        for (const auto& slot : registrations_.entries()) {
            // Paused subscriptions are absent from the OS array, including HUP/error.
            if (!slot || slot->interest == 0) continue;
            fds[count].fd = slot->fd;
            if (slot->interest & Read) fds[count].events |= read_bit;
            if (slot->interest & Write) fds[count].events |= write_bit;
            ids[count] = slot->id;
            ++count;
        }
#ifdef _WIN32
        const int n = ::WSAPoll(fds.data(), static_cast<ULONG>(count), timeout_ms);
#else
        const int n = ::poll(fds.data(), static_cast<nfds_t>(count), timeout_ms);
#endif
        if (n == 0) return result;
        if (n < 0) {
#ifdef _WIN32
            result.error = WSAGetLastError(); result.state = PollState::error;
#else
            result.error = errno;
            result.state = result.error == EINTR ? PollState::interrupted : PollState::error;
#endif
            return result;
        }
        result.state = PollState::events;
        if (fds[0].revents != 0) {
            result.woken = true;
            std::uint8_t bytes[64];
            for (unsigned i = 0; i < 16; ++i) {
                const auto read = try_receive(wake_read_, bytes, sizeof(bytes));
                if (read.state == ReceiveState::would_block) break;
                if (read.state == ReceiveState::interrupted) continue;
                if (read.state != ReceiveState::progress) {
                    result.state = PollState::error; result.error = read.error; return result;
                }
            }
        }
        for (std::size_t i = 1; i < count; ++i) {
            const short flags = fds[i].revents;
            if (flags == 0) continue;
            result.events.push_back({ids[i], (flags & (read_bit | POLLHUP)) != 0,
                                     (flags & write_bit) != 0,
                                     (flags & (POLLERR | POLLHUP | POLLNVAL)) != 0});
        }
        return result; // Snapshot: caller revalidates IDs at dispatch time.
    }
 private:
    Registrations registrations_;
    Socket wake_send_, wake_read_;
};
} // namespace
std::unique_ptr<StudyReactor> make_poll_reactor() {
    try { return std::make_unique<PollReactor>(); }
    catch (const std::exception&) { return nullptr; }
}
} // namespace study_net
