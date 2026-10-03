#ifndef STUDY_NET_IOCP_RECEIVE_H
#define STUDY_NET_IOCP_RECEIVE_H
#include "net/socket.h"
#include "net/completion_rules.h"
#include <memory>
#include <optional>
#if defined(_WIN32)
namespace study_net {
enum class PacketState { timeout, woken, result, error };
struct PacketReport { PacketState state; std::uint32_t error = 0; };
struct SubmissionReport {
    Submission state = Submission::rejected;
    std::uint32_t error = 0;
    bool accepted() const noexcept { return state != Submission::rejected; }
};
enum class CancelState { requested, not_found, no_pending, error };
struct CancelReport { CancelState state; std::uint32_t error = 0; };
// Owns one fresh, connected, overlapped-capable TCP socket; Runtime outlives it.
// Default IOCP completion notification must be enabled (no skip-on-success flag).
// Single owner thread for post/poll/take/cancel. Only wake allows another ordinary
// thread; stop/join wake callers before destruction. No signal-handler contract.
class IocpReceiver {
 public:
    explicit IocpReceiver(Socket socket);
    ~IocpReceiver(); // Cancels and collects an accepted operation before freeing storage.
    IocpReceiver(const IocpReceiver&) = delete;
    IocpReceiver& operator=(const IocpReceiver&) = delete;
    IocpReceiver(IocpReceiver&&) = delete;
    IocpReceiver& operator=(IocpReceiver&&) = delete;
    SubmissionReport post(std::uint64_t request, std::size_t capacity); // 1..16; one uncollected result.
    PacketReport poll(int timeout_ms); // 0..1000; timeout leaves the operation outstanding.
    std::optional<CompletionResult> take(); // Owned copy; only ready results can be taken.
    CancelReport request_cancel(); // Does not collect or free the operation.
    bool wake() noexcept;
 private:
    PacketReport collect(std::uint32_t timeout_ms);
    struct State;
    std::unique_ptr<State> state_;
};
}
#endif
#endif
