// server/match_seed.h — MATCH_FOUND 로 나가는 매치 seed 전용 생성기 (릴레이 공용)
//
// 이 값은 두 클라이언트에게 그대로 전달된다. 그래서 생성기 "스트림"에서 뽑으면
// 안 된다. xorshift64 는 GF(2) 위에서 선형이고 반환값이 곧 내부 상태라, 한 판을
// 마친 사람이 받은 seed 에 같은 세 줄을 적용하는 것만으로 그 뒤 서버 전체에서
// 생성되는 모든 매치의 seed 를 순서대로 계산할 수 있다. 브루트포스가 아니라
// 산술 한 번이고, 프로세스가 사는 내내 스트림이 하나뿐이라 끝없이 이어진다.
//
// 공개된 가역 변환을 덧씌우는 것으로는 막히지 않는다 — SplitMix64 finalizer 처럼
// 잘 알려진 마무리 함수는 전단사라 역산할 수 있고, 역산이 되면 선형성이 그대로
// 남는다. 관측값 두 개만 있으면 상태가 복원된다.
//
// 그래서 매치마다 노출되지 않는 엔트로피에서 새로 뽑는다. 연속한 두 seed 사이에
// 계산 가능한 관계가 없어야 한다는 것이 이 클래스가 지키는 성질이다. 매치 생성은
// 드문 사건이라(많아야 초당 몇 건) 비용은 문제가 되지 않는다.
//
// 룸 코드는 같은 이유로 이미 같은 선택을 했다 (reactor_relay.cpp 의 code_rng_,
// room.cpp 의 code_rng_state_). 규칙은 하나다 — 밖으로 나가는 값은 스트림에서
// 뽑지 않는다.

#pragma once

#include <chrono>
#include <cstdint>
#include <random>

namespace relay {

class MatchSeedSource {
public:
    // random_device 는 여기서 한 번만 만든다. 열 수 없는 환경이면 기동 시점에
    // 실패하는 편이 매치 도중에 던지는 것보다 낫다.
    MatchSeedSource()
    {
        // random_device 가 빈약한 플랫폼에서도 예측 가능한 값으로 떨어지지 않도록
        // 출처를 섞어 프로세스마다 다른 비공개 상수를 만든다. 이 값은 어디로도
        // 나가지 않으며, 로그에도 남기지 않는다.
        secret_  = (static_cast<uint64_t>(rd_()) << 32) ^ rd_();
        secret_ ^= now_ticks();
        secret_ ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
    }

    MatchSeedSource(const MatchSeedSource&)            = delete;
    MatchSeedSource& operator=(const MatchSeedSource&) = delete;

    // 매치 하나에 쓸 seed.
    uint64_t next()
    {
        uint64_t s = (static_cast<uint64_t>(rd_()) << 32) ^ rd_();
        s ^= now_ticks();
        s ^= secret_;
        s = mix(s);
        // 0 은 클라이언트 쪽 RNG 가 흡수 상태로 삼을 수 있어 피한다.
        return s ? s : 0x9E3779B97F4A7C15ULL;
    }

private:
    static uint64_t now_ticks()
    {
        return static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }

    // 비트 뭉침을 푸는 마무리 단계. 예측을 막는 것은 이 함수가 아니라 매번 새로
    // 들어오는 엔트로피다 — 이 변환 자체는 가역이며 그래도 무방하다.
    static uint64_t mix(uint64_t z)
    {
        z ^= z >> 30; z *= 0xBF58476D1CE4E5B9ULL;
        z ^= z >> 27; z *= 0x94D049BB133111EBULL;
        z ^= z >> 31;
        return z;
    }

    std::random_device rd_;
    uint64_t           secret_ = 0;
};

}  // namespace relay
