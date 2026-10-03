#pragma once
#include <cstdint>
#include <limits>

// 단일 소유자/발급기 수명 안에서만 고유하다. 0은 영구 소진을 뜻한다.
// 발급 후 다른 단계가 실패해도 번호를 되돌리지 않는다. 외부 인증 토큰이 아니다.
namespace relay {
class MonotonicId {
public:
    // Seed is exposed so boundary tests can start near exhaustion. Production
    // callers rely on the default of 1; passing 0 constructs an already
    // exhausted issuer.
    explicit constexpr MonotonicId(std::uint32_t first = 1) noexcept
        : next_(first) {}

    // Returns the next identifier, or 0 when exhausted. On success the internal
    // cursor advances. Returning std::uint32_t::max permanently exhausts the issuer
    // instead of wrapping around to 1, so values are never silently reused.
    constexpr std::uint32_t take() noexcept {
        if (next_ == 0) {
            return 0;  // Permanently exhausted; caller must handle failure.
        }
        const std::uint32_t current = next_;
        if (current == std::numeric_limits<std::uint32_t>::max()) {
            next_ = 0;  // Last value was issued; never wrap back to 1.
        } else {
            next_ = current + 1;
        }
        return current;
    }

    // No reset() is provided: exhausted issuers stay exhausted.
    MonotonicId(const MonotonicId&) = delete;
    MonotonicId& operator=(const MonotonicId&) = delete;

    // The deleted copy operations above also suppress the implicit move
    // operations, so an issuer is an identity that cannot be cloned and has
    // exactly one owner.

private:
    std::uint32_t next_;
};

} // namespace relay
