#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include "net/iocp_receive.h"
#include <cstdio>
#include <system_error>
namespace study_net {
namespace {
constexpr ULONG_PTR receive_key = 1, wake_key = 2;
enum class Phase { idle, pending, ready };
}
struct IocpReceiver::State {
    explicit State(Socket owned) : socket(std::move(owned)) {}
    ~State() { if (port) ::CloseHandle(port); }
    Socket socket;
    HANDLE port = nullptr;
    OVERLAPPED overlapped{};
    std::array<std::uint8_t,16> buffer{};
    std::uint64_t request = 0;
    std::size_t capacity = 0;
    Phase phase = Phase::idle;
    CompletionResult result{};
};
IocpReceiver::IocpReceiver(Socket socket) : state_(std::make_unique<State>(std::move(socket))) {
    if (!state_->socket.valid()) throw std::invalid_argument("valid connected socket required");
    state_->port = ::CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(static_cast<SOCKET>(state_->socket.native())),
        nullptr, receive_key, 1);
    if (!state_->port) {
        const auto error = ::GetLastError();
        throw std::system_error(static_cast<int>(error),std::system_category(),"associate IOCP");
    }
}
SubmissionReport IocpReceiver::post(std::uint64_t request, std::size_t capacity) {
    auto& state = *state_;
    if (state.phase != Phase::idle || request == 0 || capacity == 0 || capacity > state.buffer.size())
        return {Submission::rejected, ERROR_INVALID_PARAMETER};
    state.overlapped = {};
    state.buffer.fill(0);
    state.request = request;
    state.capacity = capacity;
    WSABUF bytes{static_cast<ULONG>(capacity), reinterpret_cast<char*>(state.buffer.data())};
    DWORD flags = 0;
    // WSABUF describes persistent buffer storage; Winsock captures the descriptor.
    const int rc = ::WSARecv(static_cast<SOCKET>(state.socket.native()), &bytes, 1,
                             nullptr, &flags, &state.overlapped, nullptr);
    const int error = rc == 0 ? 0 : ::WSAGetLastError();
    const auto submitted = classify_submission(rc, error, WSA_IO_PENDING);
    if (submitted != Submission::rejected) state.phase = Phase::pending;
    return {submitted, submitted == Submission::rejected ? static_cast<std::uint32_t>(error) : 0u};
}
PacketReport IocpReceiver::poll(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 1000) return {PacketState::error,ERROR_INVALID_PARAMETER};
    return collect(static_cast<std::uint32_t>(timeout_ms));
}
PacketReport IocpReceiver::collect(std::uint32_t timeout_ms) {
    auto& state = *state_;
    if (state.phase == Phase::ready) return {PacketState::result};
    DWORD count = 0;
    ULONG_PTR key = 0;
    OVERLAPPED* operation = nullptr;
    const BOOL success = ::GetQueuedCompletionStatus(state.port,&count,&key,&operation,timeout_ms);
    const DWORD error = success ? ERROR_SUCCESS : ::GetLastError();
    // FALSE + non-null OVERLAPPED is a failed I/O completion, not an empty queue.
    if (!operation) {
        if (!success) return {error == WAIT_TIMEOUT ? PacketState::timeout : PacketState::error,error};
        if (key == wake_key) return {PacketState::woken};
        return {PacketState::error,ERROR_INVALID_DATA};
    }
    if (key != receive_key || operation != &state.overlapped || state.phase != Phase::pending)
        return {PacketState::error,ERROR_INVALID_DATA};
    state.result = {};
    state.result.request = state.request;
    state.result.kind = classify_completion(success != FALSE,count,error,ERROR_OPERATION_ABORTED);
    state.result.error = error;
    if (success && count > state.capacity) {
        state.result.kind = CompletionKind::error;
        state.result.error = ERROR_INVALID_DATA;
    } else if (state.result.kind == CompletionKind::data) {
        state.result.count = count;
        state.result.bytes = state.buffer; // Value ownership survives the next submission.
    }
    state.phase = Phase::ready;
    return {PacketState::result};
}
std::optional<CompletionResult> IocpReceiver::take() {
    if (state_->phase != Phase::ready) return std::nullopt;
    const auto result = state_->result;
    state_->phase = Phase::idle;
    return result;
}
CancelReport IocpReceiver::request_cancel() {
    if (state_->phase != Phase::pending) return {CancelState::no_pending};
    if (::CancelIoEx(reinterpret_cast<HANDLE>(static_cast<SOCKET>(state_->socket.native())),
                     &state_->overlapped)) return {CancelState::requested};
    const DWORD error = ::GetLastError();
    return {error == ERROR_NOT_FOUND ? CancelState::not_found : CancelState::error,error};
}
bool IocpReceiver::wake() noexcept {
    return ::PostQueuedCompletionStatus(state_->port,0,wake_key,nullptr) != FALSE;
}
IocpReceiver::~IocpReceiver() {
    if (state_->phase != Phase::pending) return;
    (void)request_cancel();
    while (state_->phase == Phase::pending) {
        const auto packet = collect(INFINITE);
        if (packet.state == PacketState::error) {
            // Completion cannot be proved after a broken completion channel. Keep
            // socket, OVERLAPPED, buffer and port alive until process exit.
            std::fputs("IOCP shutdown failed; pending receive storage retained\n",stderr);
            (void)state_.release();
            return;
        }
    }
}
}
#endif
