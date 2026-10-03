// net/reactor_iocp.cpp — Reactor 의 Windows(IOCP) 백엔드
//
// IOCP 는 완료(completion) 모델이다: 버퍼를 미리 걸어 두면 OS 가 "다 했다"고
// 통지한다. 반면 이 인터페이스는 준비성(readiness) 모델이다. 둘을 잇는 표준 기법이
// zero-byte WSARecv 다 — 길이 0짜리 수신을 걸어 두면 커널은 실제로 바이트를
// 복사하지 않는 수신의 완료를 통지한다. 데이터·EOF·오류를 알리는 읽기 힌트로
// 변환하며, 이후 net/socket.cpp 의 논블로킹 recv 결과에서 실제 상태를 판단한다.
// 통지 자체가 바이트를 예약하지 않으므로 WouldBlock도 정상적으로 처리한다.
//
// 쓰기 준비성의 한계 (정직하게 밝힌다)
//   여기서는 kWrite 관심 fd의 writable을 실제 OS 준비성 확인 없이 재시도 힌트로
//   합성한다. 리스너/폴백/쓰기 관심이 있으면 요청 대기를 최대25ms로 줄인다.
//   실제 실행 시각의 상한은 아니며 다른 완료가 잦으면 재시도도 잦아질 수 있다.
//   부하와 송신 큐 압박을 측정하고 필요하면 overlapped WSASend 경로를 설계한다.

#if defined(_WIN32)

#include "reactor.h"

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace net {

namespace {

// IOCP 완료 키는 소켓 연산과 wake의 종류를 구분한다. 개별 요청은 OVERLAPPED로 찾는다.
constexpr ULONG_PTR kWakeKey = 1;
constexpr ULONG_PTR kSockKey = 2;

struct SockState {
    WSAOVERLAPPED ov{};        // zero-byte WSARecv 용. CONTAINING_RECORD 로 역참조.
    void*    token       = nullptr;
    NativeSocket fd = kInvalidSocket;     // 완료에서 되짚어 재무장할 때 필요하다
    unsigned interest    = 0;
    bool     read_armed  = false;  // zero-byte recv 가 걸려 있는가
    bool     arm_queued  = false;  // 재무장 대기열에 이미 들어 있는가(중복 방지)
    bool     poll_always = false;  // 무장 불가(리스너 등) — 매 poll 보고
    char     dummy       = 0;      // 길이 0 버퍼의 앵커
};

class IocpReactor final : public Reactor {
public:
    bool init() {
        iocp_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        return iocp_ != nullptr;
    }

    ~IocpReactor() override {
        // A cancellation request does not release the kernel's OVERLAPPED loan.
        // Include still-registered sockets, not only states retired by remove().
        size_t pending = zombies_.size();
        for (auto& entry : socks_) {
            SockState& st = *entry.second;
            if (!st.read_armed) continue;
            ++pending;
            ::CancelIoEx(reinterpret_cast<HANDLE>(static_cast<SOCKET>(st.fd)), &st.ov);
        }
        // Removed states have already had cancellation requested. A completed
        // operation may still have its packet queued even if CancelIoEx found none.
        while (pending != 0) {
            ULONG got = 0;
            OVERLAPPED_ENTRY batch[32];
            if (!::GetQueuedCompletionStatusEx(iocp_, batch, 32, &got, INFINITE, FALSE)) {
                if (::GetLastError() == WAIT_TIMEOUT) continue;
                // If the completion channel is irrecoverably broken, completion
                // cannot be proven. Retain only in-flight storage until process
                // exit instead of freeing memory that the OS may still access.
                std::fputs("[reactor] IOCP shutdown failed; pending I/O storage retained\n", stderr);
                for (auto& entry : socks_)
                    if (entry.second->read_armed) (void)entry.second.release();
                for (auto& entry : zombies_) (void)entry.second.release();
                break;
            }
            for (ULONG i = 0; i < got; ++i) {
                if (batch[i].lpCompletionKey != kSockKey || !batch[i].lpOverlapped) continue;
                SockState* st = CONTAINING_RECORD(batch[i].lpOverlapped, SockState, ov);
                auto retired = zombies_.find(st);
                if (retired != zombies_.end()) {
                    zombies_.erase(retired);
                    --pending;
                } else if (st->read_armed) {
                    st->read_armed = false;
                    --pending;
                }
            }
        }
        if (iocp_) ::CloseHandle(iocp_);
    }

    bool add(NativeSocket fd, unsigned interest, void* token) override {
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
        st->fd       = fd;
        st->interest = interest;
        // listen 소켓은 zero-byte WSARecv 를 받지 못한다(수신 큐가 아니라 백로그를
        // 가진 소켓이다). IOCP 로 accept 준비성을 제대로 얻으려면 AcceptEx 로 수락
        // 소켓을 미리 걸어 둬야 한다. 이 어댑터는 수락된 소켓을 보관/인계하는
        // AcceptEx 경로 대신 이런 소켓을 매 poll 마다 readable
        // 로 보고하고 호출자가 accept 를 시도하게 둔다 — 대상이 리스너 몇 개뿐이라
        // 비용이 연결 수에 비례하지 않는다.
        int listening = 0;
        int optlen = sizeof(listening);
        if (::getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN,
                         reinterpret_cast<char*>(&listening), &optlen) == 0 && listening) {
            st->poll_always = true;
        }
        SockState* raw = st.get();
        socks_[fd] = std::move(st);
        if (raw->poll_always) poll_always_.insert(fd);
        track_interest(raw);
        return true;
    }

    bool modify(NativeSocket fd, unsigned interest, void* token) override {
        auto it = socks_.find(fd);
        if (it == socks_.end()) return false;
        it->second->token    = token;
        it->second->interest = interest;
        track_interest(it->second.get());
        return true;
    }

    bool remove(NativeSocket fd) override {
        auto it = socks_.find(fd);
        if (it == socks_.end()) return false;

        SockState* raw = it->second.get();
        if (raw->read_armed) {
            // 커널이 아직 &raw->ov 를 들고 있다. 여기서 상태 객체를 해제하면 뒤늦게
            // 도착하는 완료 통지의 CONTAINING_RECORD 가 해제된 메모리를 가리킨다
            // (use-after-free). 취소를 요청하고, 그 완료를 회수할 때까지 객체를
            // 살려 둔다. 취소는 요청일 뿐이며 완료가 이미 큐에 있을 수도 있다.
            // 기본 완료 통지 설정에서 수락한 연산의 패킷을 회수한 뒤 해제한다.
            ::CancelIoEx(reinterpret_cast<HANDLE>(static_cast<SOCKET>(fd)), &raw->ov);
            zombies_.emplace(raw, std::move(it->second));
        }
        write_interest_.erase(fd);
        poll_always_.erase(fd);
        // need_arm_ 에 남은 항목은 드레인 때 socks_ 조회로 걸러진다(지연 무효화).
        socks_.erase(it);
        return true;
    }

    int poll(std::vector<Event>& out, int timeout_ms) override {
        out.clear();

        // 1) 재무장이 필요한 소켓에만 zero-byte recv 를 건다. 전체를 훑지 않는 것이
        //    등록된 전체 연결 대신 재무장 후보를 처리한다. 지연 무효화된 fd는
        //    아래 조회로 거르고, 실제 비용에는 폴백과 쓰기 관심 처리도 포함된다.
        for (size_t i = 0; i < need_arm_.size(); ++i) {
            const NativeSocket fd = need_arm_[i];
            auto it = socks_.find(fd);
            if (it == socks_.end()) continue;           // remove() 된 낡은 항목
            SockState& st = *it->second;
            st.arm_queued = false;
            if ((st.interest & kRead) && !st.read_armed) arm_read(fd, st, out);
        }
        need_arm_.clear();

        // 2) 보류 송신이 있으면 유휴 스핀을 피하되 재시도가 늦지 않게 타임아웃을 죈다.
        const bool any_write = !write_interest_.empty() || !poll_always_.empty();
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
            // remove() 로 이미 떠난 연결의 취소 완료라면 여기서 회수하고 버린다.
            auto zit = zombies_.find(st);
            if (zit != zombies_.end()) {
                zombies_.erase(zit);
                continue;
            }
            st->read_armed = false;      // 이 완료로 무장이 소진됐다
            queue_arm(st);               // 다음 poll 에서 다시 걸도록 예약

            // 무장을 건 뒤 관심에서 kRead 가 빠졌다면(루프가 backpressure 로 읽기를
            // 멈춘 경우) 이 완료는 알리지 않는다. 이미 제출한 연산의 회수와
            // 현재 읽기 정책은 별개다. 그대로 내보내면 읽지 말라고
            // 한 소켓에서 한 번 더 읽게 되어 backpressure 가 새어 나간다.
            if (!(st->interest & kRead)) continue;

            Event ev;
            ev.token    = st->token;
            ev.readable = true;  // zero-byte recv 완료 = 읽을 수 있음(또는 EOF/에러)
            // OVERLAPPED_ENTRY::Internal은 예약 필드다. 완료 패킷을 꺼낸 뒤
            // 문서화된 Winsock API로 해당 연산 결과를 확인한다(추가 대기 없음).
            DWORD transferred = 0, flags = 0;
            ev.error = !::WSAGetOverlappedResult(static_cast<SOCKET>(st->fd), &st->ov,
                                                &transferred, FALSE, &flags);
            out.push_back(ev);
        }

        synth_writable(out);
        return static_cast<int>(out.size());
    }

    // 소켓 핸들은 CreateIoCompletionPort 로 한 번 결합되면 수명 동안 그 포트에
    // 묶인다. 다른 포트로 다시 결합할 방법이 없으므로 루프 간 이동을 지원하지 않는다.
    bool can_migrate_sockets() const override { return false; }

    void wake() override {
        // IOCP 는 PostQueuedCompletionStatus 를 스레드 안전하게 보장한다.
        ::PostQueuedCompletionStatus(iocp_, 0, kWakeKey, nullptr);
    }

private:
    static constexpr DWORD kWritePollMs = 25;

    void arm_read(NativeSocket fd, SockState& st, std::vector<Event>& out) {
        WSABUF buf;
        buf.buf = &st.dummy;
        buf.len = 0;  // zero-byte: 데이터를 소비하지 않고 준비성만 감지
        DWORD flags = 0;
        std::memset(&st.ov, 0, sizeof(st.ov));
        int r = ::WSARecv(static_cast<SOCKET>(fd), &buf, 1, nullptr, &flags,
                          &st.ov, nullptr);
        if (r == 0) {
            // 즉시 완료 — 그래도 완료 통지가 IOCP 로 큐잉되므로 무장 상태로 둔다.
            st.read_armed = true;
        } else if (::WSAGetLastError() == WSA_IO_PENDING) {
            st.read_armed = true;
        } else {
            // 무장 실패 — 즉시 readable+error 로 노출해 루프가 확정 처리하게 한다.
            // 여기서 끝내면 이 fd 는 다시 무장되지 않아 조용히 죽은 소켓이 되므로,
            // 매 poll 보고 대상으로 돌려 호출자가 상태를 확인할 기회를 계속 준다.
            st.poll_always = true;
            poll_always_.insert(fd);
            Event ev;
            ev.token = st.token;
            ev.readable = true;
            ev.error = true;
            out.push_back(ev);
        }
    }

    void synth_writable(std::vector<Event>& out) {
        // kWrite 관심 fd 에 낙관적 writable 을 합성한다(위 주석의 한계 참조).
        // 이미 이번 배치에 이벤트가 있는 token 은 그 자리에 writable 을 합친다 —
        // epoll 백엔드가 fd 하나당 이벤트 하나를 돌려주므로 동작을 맞춘다. 그렇지
        // 않으면 같은 연결이 배치 안에 두 번 나타나 루프가 백엔드마다 다른 형태를
        // 처리해야 한다.
        if (write_interest_.empty() && poll_always_.empty()) return;
        std::unordered_map<void*, size_t> idx;
        for (size_t i = 0; i < out.size(); ++i) idx.emplace(out[i].token, i);

        // 이미 이번 배치에 이벤트가 있으면 그 자리에 비트를 합치고, 없으면 새로 넣는다.
        auto mark = [&](SockState& st, bool readable, bool writable) {
            auto it = idx.find(st.token);
            if (it != idx.end()) {
                if (readable) out[it->second].readable = true;
                if (writable) out[it->second].writable = true;
                return;
            }
            Event ev;
            ev.token    = st.token;
            ev.readable = readable;
            ev.writable = writable;
            idx.emplace(st.token, out.size());
            out.push_back(ev);
        };

        // 쓰기 관심이 걸린 소켓만 본다 — 보류 송신은 드문 상태라 이 집합은 대개 비어 있다.
        for (NativeSocket fd : write_interest_) {
            auto sit = socks_.find(fd);
            if (sit == socks_.end()) continue;
            mark(*sit->second, false, true);
        }
        // 무장할 수 없는 소켓(리스너)은 준비성을 알 방법이 없으므로 매번 알린다.
        for (NativeSocket fd : poll_always_) {
            auto sit = socks_.find(fd);
            if (sit == socks_.end()) continue;
            SockState& st = *sit->second;
            if (st.interest & kRead) mark(st, true, false);
        }
    }

    // 관심 변화를 두 색인에 반영한다. 재무장 대기열은 중복 없이 한 번만 쌓는다.
    void track_interest(SockState* st) {
        if (st->interest & kWrite) write_interest_.insert(st->fd);
        else                       write_interest_.erase(st->fd);
        if ((st->interest & kRead) && !st->read_armed) queue_arm(st);
    }

    void queue_arm(SockState* st) {
        if (st->arm_queued || st->poll_always) return;
        if (!(st->interest & kRead)) return;
        st->arm_queued = true;
        need_arm_.push_back(st->fd);
    }

    HANDLE iocp_ = nullptr;
    std::unordered_map<NativeSocket, std::unique_ptr<SockState>> socks_;
    // remove() 됐지만 커널이 아직 OVERLAPPED 를 들고 있는 상태 객체. 취소 완료가
    // 회수될 때까지만 살아 있다.
    std::unordered_map<SockState*, std::unique_ptr<SockState>> zombies_;
    // poll 을 O(등록 수)가 아니라 O(할 일)로 유지하는 두 색인.
    std::vector<NativeSocket> need_arm_;        // 재무장 대기 (지연 무효화)
    std::unordered_set<NativeSocket> write_interest_;  // kWrite 가 걸린 소켓
    std::unordered_set<NativeSocket> poll_always_;     // 리스너 등 무장 불가 소켓
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
