// server/ip_admission.h — per-IP 입장 제어 (두 릴레이 바이너리 공용)
//
// 상한이 둘이다. 하나만으로는 둘 다 못 막는다.
//
//   kMaxHandshakesPerIp — accept 부터 "인증이 끝나는 순간"까지만 잡는 슬롯.
//       아직 자기가 누구인지 밝히지 않은 연결이 한 주소에서 몇 개까지 동시에
//       열려 있을 수 있는지를 정한다. 첫 프레임을 안 보내고 버티거나 토큰 검증
//       왕복만 반복해 접속 처리 경로를 점유하는 부하를 막는 것이 목적이므로,
//       진로가 정해지는 즉시(after_auth) 놓아준다. 이 슬롯을 연결 수명 동안
//       붙들면 상한 16 이 "IP당 동시 세션 16" 이 되어, IP를 공유하는 정상
//       사용자 집단(통신사 CGNAT, PC방 LAN)에서 17번째 사람이 거절된다.
//
//   kMaxSessionsPerIp — accept 부터 "연결이 죽을 때까지" 잡는 슬롯.
//       인증을 통과한 뒤에도 유지되므로 한 주소가 서버 전체를 차지하는 경로를
//       막는다. 핸드셰이크 슬롯만 두면 인증만 통과시키며 전역 상한까지 무제한
//       으로 쌓을 수 있다 — 상한이 아니라 속도 제한일 뿐이다.
//
// 두 상한은 독립적으로 검사한다. 어느 하나라도 못 얻으면 그 연결은 거절이다.
//
// 슬롯은 RAII 다. acquire() 가 돌려준 shared_ptr 의 마지막 사본이 사라질 때
// 카운터가 줄어든다. 그래서 세션 슬롯은 소켓과 함께 큐·룸·포워딩 채널로
// 옮겨 다니며(PlayerSessionLease 와 같은 방식), 어느 스레드에서 마지막
// 사본이 죽든 정확히 한 번 반납된다. 표가 프로세스 전역이라 reactor 의
// 샤드 스레드가 인계받은 연결을 닫아도 앞단이 센 수가 함께 줄어든다.

#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace relay {

// 동시 핸드셰이크 예산. 인증이 끝나면 즉시 반납되므로 짧게 잡아도 된다.
constexpr size_t kMaxHandshakesPerIp = 16;

// 동시 세션 예산의 기본값. 근거:
//
//   · 전역 상한 대비 — 두 바이너리 모두 동시 연결이 대략 512 에서 멎는다
//     (reactor 는 kMaxConns=512, 스레드 모델은 포워딩 워커 512개를 매치당 2개씩
//     쓰므로 512 연결). 64 는 그 1/8 이라 한 주소가 서버를 다 채우려면 최소 8개
//     주소가 필요하다 — 단일 출처의 독식이 성립하지 않는다.
//   · 핸드셰이크 상한 대비 — 16 의 4배. 세션 예산이 핸드셰이크 예산보다 작으면
//     핸드셰이크 예산을 다 쓸 수조차 없으므로 반드시 더 커야 하고, 인증을 마친
//     세션이 자리를 잡은 뒤에도 같은 주소의 새 접속이 들어올 여지를 남긴다.
//   · 정상 수요 대비 — 한 공인 주소를 공유하는 집단(CGNAT 풀, PC방 LAN)이 이
//     게임 하나에 동시에 64명을 붙이는 상황은 이 규모의 서버에서 현실적이지
//     않다. 그보다 큰 곳을 실제로 만나면 --max-sessions-per-ip 로 올리면 된다.
//     기본값을 미리 크게 잡아 두는 것보다 이쪽이 안전하다 — 기본값이 느슨하면
//     아무도 그 사실을 모르는 채로 독식이 가능해진다.
//
// 이 값은 python/tests/test_relay_meta_smoke.py 의
// RELAY_MAX_SESSIONS_PER_IP 와 같아야 한다 (회귀 테스트가 경계를 못 박는다).
constexpr size_t kMaxSessionsPerIp = 64;

class IpAdmission {
public:
    enum class Kind { Handshake, Session };

    // 기동 시 인자 파싱 직후 한 번만 호출한다 (accept 시작 전).
    static void set_session_limit(size_t n)
    {
        std::lock_guard<std::mutex> lk(mu_);
        session_limit_ = (n == 0) ? 1 : n;
    }

    static size_t session_limit()
    {
        std::lock_guard<std::mutex> lk(mu_);
        return session_limit_;
    }

    // 슬롯 하나를 잡는다. 상한에 걸리면 nullptr — 호출자는 연결을 거절한다.
    // key 는 보통 peer IP. getpeername 이 실패했을 때 모든 실패 연결이 하나의
    // 버킷을 공유해 서로를 굶기지 않도록, 호출자가 연결마다 고유한 키를 대신
    // 넘길 수 있다 (per-IP 상한은 못 걸지만 공멸보다는 낫다).
    static std::shared_ptr<IpAdmission> acquire(std::string key, Kind kind)
    {
        if (key.empty()) key = "unknown";
        std::lock_guard<std::mutex> lk(mu_);
        auto&        table = (kind == Kind::Handshake) ? handshakes_ : sessions_;
        const size_t limit = (kind == Kind::Handshake) ? kMaxHandshakesPerIp
                                                       : session_limit_;
        auto         it    = table.find(key);
        const size_t n     = (it == table.end()) ? 0u : it->second;
        if (n >= limit) return {};
        table[key] = n + 1;
        return std::shared_ptr<IpAdmission>(new IpAdmission(std::move(key), kind));
    }

    ~IpAdmission()
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto& table = (kind_ == Kind::Handshake) ? handshakes_ : sessions_;
        auto  it    = table.find(key_);
        if (it == table.end()) return;
        if (--it->second == 0) table.erase(it);
    }

    IpAdmission(const IpAdmission&) = delete;
    IpAdmission& operator=(const IpAdmission&) = delete;

private:
    IpAdmission(std::string key, Kind kind)
        : key_(std::move(key)), kind_(kind) {}

    std::string key_;
    Kind        kind_;

    inline static std::mutex                             mu_;
    inline static std::unordered_map<std::string, size_t> handshakes_;
    inline static std::unordered_map<std::string, size_t> sessions_;
    inline static size_t                                  session_limit_ =
        kMaxSessionsPerIp;
};

}  // namespace relay
