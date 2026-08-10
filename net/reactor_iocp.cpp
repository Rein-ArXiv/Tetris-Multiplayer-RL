// net/reactor_iocp.cpp — Reactor 의 Windows(IOCP) 백엔드
//
// IOCP 는 완료(completion) 모델이다: 버퍼를 미리 걸어 두면 OS 가 "다 했다"고
// 통지한다. 반면 이 인터페이스는 준비성(readiness) 모델이다. 둘을 잇는 표준 기법이
// zero-byte WSARecv 다 — 길이 0짜리 수신을 걸어 두면 커널은 실제로 바이트를
// 복사하지 않고, "지금 recv 하면 WOULDBLOCK 없이 진행된다(데이터 있음 또는 EOF)"가
// 되는 순간 완료를 통지한다. 그 통지가 곧 read 준비성이고, 이후 실제 recv 는
// net/socket.cpp 의 논블로킹 경로로 즉시 성공한다.
//
// 쓰기 준비성의 한계 (정직하게 밝힌다)
//   IOCP 에는 write 준비성 통지가 없다. 릴레이 송신은 작고 커널 송신 버퍼가 차는
//   일이 드물어, 여기서는 kWrite 관심이 걸린 fd 에 대해 매 poll 마다 낙관적으로
//   writable 을 합성해 돌려준다(루프가 실제 send 를 시도해 확인). 보류 송신이 있을
//   때만 짧은 타임아웃으로 재시도하므로 유휴 시 스핀은 없다. 대량 backpressure 가
//   상시적인 워크로드라면 완료(proactor) 모델로 send 자체를 IOCP 에 거는 편이 맞다.

#if defined(_WIN32)

#include "reactor.h"

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include <cstring>
#include <unordered_map>
#include <vector>

namespace net {

namespace {

// IOCP 완료 키. 소켓은 자기 token 을 키로 연결하고, wake 는 전용 키를 쓴다.
constexpr ULONG_PTR kWakeKey = 1;
constexpr ULONG_PTR kSockKey = 2;

struct SockState {
    WSAOVERLAPPED ov{};        // zero-byte WSARecv 용. CONTAINING_RECORD 로 역참조.
    void*    token       = nullptr;
    unsigned interest    = 0;
    bool     read_armed  = false;  // zero-byte recv 가 걸려 있는가
    char     dummy       = 0;      // 길이 0 버퍼의 앵커
};

class IocpReactor final : public Reactor {
public:
    bool init() {
        iocp_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        return iocp_ != nullptr;
    }

    ~IocpReactor() override {
        if (iocp_) ::CloseHandle(iocp_);
    }

    bool add(int fd, unsigned interest, void* token) override {
        if (socks_.count(fd)) return false;
        SOCKET s = static_cast<SOCKET>(fd);
        // 소켓을 완료 포트에 연결. 키로 소켓 완료임을 구분한다(개별 상태는 OVERLAPPED
        // 를 CONTAINING_RECORD 로 되짚어 얻는다).
        if (::CreateIoCompletionPort(reinterpret_cast<HANDLE>(s), iocp_,
                                     kSockKey, 0) == nullptr) {
            return false;
        }
        auto st = std::make_unique<SockState>();
        st->token    = token;
        st->interest = interest;
        socks_[fd]   = std::move(st);
        return true;
    }

    bool modify(int fd, unsigned interest, void* token) override {
        auto it = socks_.find(fd);
        if (it == socks_.end()) return false;
        it->second->token    = token;
        it->second->interest = interest;
        return true;
    }

    bool remove(int fd) override {
        // 걸려 있던 zero-byte recv 는 소켓 close 시 커널이 취소·완료시킨다. 여기서는
        // 상태만 버린다(연결 close 는 호출자의 TcpSocket RAII 소관).
        return socks_.erase(fd) > 0;
    }

    int poll(std::vector<Event>& out, int timeout_ms) override {
        out.clear();

        // 1) read 관심이 있으나 아직 안 걸린 소켓에 zero-byte recv 를 (재)무장한다.
        bool any_write = false;
        for (auto& [fd, st] : socks_) {
            if ((st->interest & kRead) && !st->read_armed) {
                arm_read(fd, *st, out);
            }
            if (st->interest & kWrite) any_write = true;
        }

        // 2) 보류 송신이 있으면 유휴 스핀을 피하되 재시도가 늦지 않게 타임아웃을 죈다.
        DWORD wait_ms = (timeout_ms < 0) ? INFINITE : static_cast<DWORD>(timeout_ms);
        if (any_write && (wait_ms == INFINITE || wait_ms > kWritePollMs)) {
            wait_ms = kWritePollMs;
        }

        // 3) 완료를 한 번에 여러 개 꺼낸다.
        if (entries_.empty()) entries_.resize(256);
        ULONG got = 0;
        BOOL ok = ::GetQueuedCompletionStatusEx(
            iocp_, entries_.data(), static_cast<ULONG>(entries_.size()),
            &got, wait_ms, FALSE);
        if (!ok) {
            DWORD err = ::GetLastError();
            if (err == WAIT_TIMEOUT) {
                synth_writable(out);
                return static_cast<int>(out.size());
            }
            return -1;
        }

        for (ULONG i = 0; i < got; ++i) {
            const OVERLAPPED_ENTRY& e = entries_[i];
            if (e.lpCompletionKey == kWakeKey) {
                continue;  // wake — 이벤트로 노출하지 않는다
            }
            SockState* st = CONTAINING_RECORD(e.lpOverlapped, SockState, ov);
            st->read_armed = false;  // 이 완료로 무장이 소진됐다
            Event ev;
            ev.token    = st->token;
            ev.readable = true;  // zero-byte recv 완료 = 읽을 수 있음(또는 EOF/에러)
            // 완료가 오류로 끝났으면 error 도 표시(다음 recv 가 확정 처리).
            if (e.Internal != 0) ev.error = true;
            out.push_back(ev);
        }

        synth_writable(out);
        return static_cast<int>(out.size());
    }

    void wake() override {
        // IOCP 는 PostQueuedCompletionStatus 를 스레드 안전하게 보장한다.
        ::PostQueuedCompletionStatus(iocp_, 0, kWakeKey, nullptr);
    }

private:
    static constexpr DWORD kWritePollMs = 25;

    void arm_read(int fd, SockState& st, std::vector<Event>& out) {
        WSABUF buf;
        buf.buf = &st.dummy;
        buf.len = 0;  // zero-byte: 데이터를 소비하지 않고 준비성만 감지
        DWORD flags = 0, recvd = 0;
        std::memset(&st.ov, 0, sizeof(st.ov));
        int r = ::WSARecv(static_cast<SOCKET>(fd), &buf, 1, &recvd, &flags,
                          &st.ov, nullptr);
        if (r == 0) {
            // 즉시 완료 — 그래도 완료 통지가 IOCP 로 큐잉되므로 무장 상태로 둔다.
            st.read_armed = true;
        } else if (::WSAGetLastError() == WSA_IO_PENDING) {
            st.read_armed = true;
        } else {
            // 무장 실패(소켓 오류) — 즉시 readable+error 로 노출해 루프가 확정 처리.
            Event ev;
            ev.token = st.token;
            ev.readable = true;
            ev.error = true;
            out.push_back(ev);
        }
    }

    void synth_writable(std::vector<Event>& out) {
        // kWrite 관심 fd 에 낙관적 writable 을 합성한다(위 주석의 한계 참조).
        for (auto& [fd, st] : socks_) {
            (void)fd;
            if (st->interest & kWrite) {
                Event ev;
                ev.token = st->token;
                ev.writable = true;
                out.push_back(ev);
            }
        }
    }

    HANDLE iocp_ = nullptr;
    std::unordered_map<int, std::unique_ptr<SockState>> socks_;
    std::vector<OVERLAPPED_ENTRY> entries_;
};

} // namespace

std::unique_ptr<Reactor> Reactor::create() {
    auto r = std::unique_ptr<IocpReactor>(new IocpReactor());
    if (!r->init()) return nullptr;
    return r;
}

} // namespace net

#endif // _WIN32
