#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace net {
// One receive worker owns this window. Tokens are the timestamps that THIS
// connection queued in PING. Matching is correlation, not authentication.
class PongWindow {
public:
    bool remember(std::uint64_t token) noexcept {
        // Never reissue a consumed or evicted timestamp within this connection.
        if (last_issued_ && token <= *last_issued_) return false;
        last_issued_ = token;
        pending_[next_] = token;
        next_ = (next_ + 1) % pending_.size();
        return true;
    }
    bool consume(std::uint64_t token, std::uint64_t now,
                 std::uint64_t max_age) noexcept {
        if (now < token || now - token >= max_age) return false;
        for (auto& item : pending_) {
            if (item && *item == token) { item.reset(); return true; }
        }
        return false;
    }
private:
    std::array<std::optional<std::uint64_t>, 16> pending_{};
    std::size_t next_ = 0;
    std::optional<std::uint64_t> last_issued_;
};
} // namespace net
