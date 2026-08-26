// server/room_guess_budget.h — per-IP 룸 코드 오답 예산
//
// 룸 코드는 5글자(32^5 ≈ 3,355만)이고, 그 방에 들어갈 자격을 증명하는 유일한
// 값이다. 그래서 "모르는 코드를 계속 넣어 보는" 행위에 값을 매겨야 한다.
//
// 이미 있는 방어가 하나 있다: 코드가 틀리면 릴레이가 그 연결을 닫는다. 그래서
// 시도 한 번에 TCP 연결 하나가 들고, per-IP 핸드셰이크 상한(16)이 동시 시도를
// 묶는다. 하지만 그건 **속도 제한이지 총량 제한이 아니다** — 붙었다 끊기를
// 반복하면 시도 횟수는 시간에 비례해 계속 늘어난다. 방이 살아 있는 동안(보통
// 친구를 기다리는 몇 십 초) 한 IP 가 던질 수 있는 추측의 수를 실제로 잘라야
// 코드 공간의 크기가 방어력으로 환산된다.
//
// 그래서 오답에만 값을 매기는 토큰 버킷을 둔다. 정답으로 들어가는 것은 세지
// 않으므로 정상 사용자는 이 예산의 존재를 알 수 없다. 사람이 코드를 잘못 받아
// 적는 경우(대문자 O 와 0 을 섞는 등)를 위해 버스트를 넉넉히 두되, 충전은 아주
// 느리게 한다 — 사람은 몇 번 틀리고 멈추지만 자동화는 멈추지 않기 때문이다.
//
// 표가 무한정 자라면 이 방어 자체가 메모리 고갈 경로가 된다. 그래서 항목 수에
// 천장을 두고, 가득 차면 이미 예산을 다 회복한(=놀고 있는) 항목부터 지운다.
// 지울 게 없으면 새 항목을 만들지 않고 통과시킨다 — 정상 사용자를 막느니
// 그 순간의 추측 하나를 허용하는 편이 낫다.

#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace relay {

// 버스트·충전 간격·추적 상한을 값으로 받는다. 인스턴스마다 표가 따로 생기므로
// 용도가 다른 예산이 서로의 항목을 밀어내지 않는다.
template <int Burst, int RefillSeconds, size_t MaxTracked = 4096>
class IpBudget {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // 사람이 손으로 틀릴 만한 횟수보다 넉넉하되, 코드 공간에 비하면 없는 것과
    // 같은 수. 이 값을 다 쓰면 충전 속도가 곧 공격 속도의 천장이 된다.
    static constexpr int kBurst = Burst;

    // 오답 하나를 회복하는 데 걸리는 시간. 12회를 쓴 뒤로는 30초에 한 번씩만
    // 더 던질 수 있다 — 3,355만 개를 훑으려면 30년 단위의 시간이 든다.
    static constexpr auto kRefillInterval = std::chrono::seconds(RefillSeconds);

    // 표 항목 수 천장. per-IP 세션 상한(64)과 달리 이건 순수한 메모리 방어다.
    static constexpr size_t kMaxTracked = MaxTracked;

    // 실패 하나를 청구한다. 예산이 남아 있으면 true(거절하지 않음).
    static bool charge(const std::string& key, TimePoint now)
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = table_.find(key);
        if (it == table_.end()) {
            if (table_.size() >= kMaxTracked) evict_one(now);
            table_.emplace(key, Entry{kBurst - 1, now});
            return true;
        }
        refill(it->second, now);
        if (it->second.tokens <= 0) return false;
        --it->second.tokens;
        return true;
    }

    // 예산이 남아 있는지만 본다 — 토큰을 쓰지 않는다.
    //
    // 소비(charge)와 확인(allowed)을 나누는 이유가 있다. 실패에 값을 매기는
    // 예산에서는 "값을 치르는 순간"과 "문을 막는 순간"이 다르다. 두 곳 모두
    // charge 를 부르면 정상 사용자의 평범한 시도까지 토큰을 깎아, 아무 잘못이
    // 없는 사람이 버스트 횟수만큼만 쓰고 막힌다.
    static bool allowed(const std::string& key, TimePoint now)
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = table_.find(key);
        if (it == table_.end()) return true;   // 이력이 없으면 가득 찬 것과 같다
        refill(it->second, now);
        return it->second.tokens > 0;
    }

    // 테스트가 경계를 확인할 수 있도록 비워 준다.
    static void reset_all()
    {
        std::lock_guard<std::mutex> lk(mu_);
        table_.clear();
    }

private:
    struct Entry {
        int       tokens;
        TimePoint refilled_at;
    };

    static void refill(Entry& e, TimePoint now)
    {
        if (now <= e.refilled_at) return;
        const auto elapsed = now - e.refilled_at;
        const auto steps   = elapsed / kRefillInterval;
        if (steps <= 0) return;
        e.refilled_at += kRefillInterval * steps;
        const long long grown = (long long)e.tokens + steps;
        e.tokens = (grown > kBurst) ? kBurst : (int)grown;
    }

    // 자리 하나를 비운다. 예산을 전부 회복한 항목이 있으면 그쪽을 먼저 지운다 —
    // 지워도 상태를 잃지 않기 때문이다(다시 오면 가득 찬 버킷으로 시작한다).
    //
    // 그런 항목이 하나도 없으면 **남은 토큰이 가장 많은** 항목을 지운다. 예전에는
    // 이 경우 아무것도 지우지 않고 새 항목도 만들지 않은 채 통과시켰는데, 그건
    // 표가 부분 소진된 항목들로 가득 차는 순간 이 방어가 통째로 꺼진다는 뜻이다 —
    // 그 뒤로는 추적되지 않는 모든 주소가 무제한으로 시도할 수 있다. 표를 채우는
    // 것 자체가 공격이 되어 버리므로, 자리는 반드시 난다. 지워서 잃는 것은 가장
    // 적게 소진된, 즉 가장 덜 의심스러운 항목의 이력뿐이다.
    static void evict_one(TimePoint now)
    {
        auto victim = table_.end();
        int  best   = -1;
        for (auto it = table_.begin(); it != table_.end(); ++it) {
            refill(it->second, now);
            if (it->second.tokens >= kBurst) { table_.erase(it); return; }
            if (it->second.tokens > best) { best = it->second.tokens; victim = it; }
        }
        if (victim != table_.end()) table_.erase(victim);
    }

    inline static std::mutex                              mu_;
    inline static std::unordered_map<std::string, Entry>  table_;
};

// 룸 코드 오답 — 사람이 받아 적다 틀리는 횟수보다 넉넉하되, 소진 뒤에는 30초에
// 하나씩만 더 던질 수 있다. 32^5 를 훑으려면 30년 단위가 든다.
using RoomGuessBudget = IpBudget<12, 30>;

// 매칭된 뒤 READY 를 끝내 보내지 않은 횟수. 큐에 조용히 서 있다가 짝이 잡히면
// 아무것도 하지 않는 연결은 애먼 상대의 로비 대기 시간을 통째로 태운다. 대기
// 자체에는 값을 매길 수 없고(한가한 서버에서 오래 기다리는 것은 정상이다) 짝이
// 잡힌 뒤의 침묵에만 값을 매길 수 있다. 정상 사용자도 그 순간 앱이 죽거나 끊길
// 수 있으므로 버스트를 두되, 반복되면 그 주소의 큐 진입을 막는다.
using LobbyNoShowBudget = IpBudget<6, 120>;

}  // namespace relay
