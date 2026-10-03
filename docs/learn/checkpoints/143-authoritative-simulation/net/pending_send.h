#pragma once
#include "net/byte_budget.h"
#include "net/send_budget.h"
#include <array>
#include <algorithm>
#include <type_traits>
namespace study_net {
enum class BufferResult { stored, closed, invalid, local_limit, global_limit };
enum class FlushResult { empty, waiting, progress, error, invalid };

// One owner mutates this ring. The shared counter must outlive every queue.
// High/low water marks advise the producer; already-read bytes may still append.
template<std::size_t Capacity, std::size_t High, std::size_t Low>
class PendingSend {
    static_assert(0 < Low && Low < High && High <= Capacity, "watermark order");
public:
    PendingSend(std::atomic<std::size_t>& total, std::size_t limit) noexcept
        : total_(total), limit_(limit) {}
    ~PendingSend() { close(); }
    PendingSend(const PendingSend&) = delete;
    PendingSend& operator=(const PendingSend&) = delete;
    std::size_t size() const noexcept { return size_; }
    std::size_t room() const noexcept { return Capacity - size_; }
    bool paused() const noexcept { return paused_; }
    bool closed() const noexcept { return closed_; }

    BufferResult append(const std::uint8_t* data, std::size_t count) noexcept {
        if (closed_) return BufferResult::closed;
        if (count && !data) return BufferResult::invalid;
        if (count > room()) return BufferResult::local_limit;
        std::size_t reserved = 0;
        if (!try_reserve_bytes(total_, limit_, count, reserved))
            return BufferResult::global_limit;
        // Inline byte copies cannot throw: after reservation there is no allocation.
        for (std::size_t n = 0; n < count; ++n)
            bytes_[(head_ + size_ + n) % Capacity] = data[n];
        size_ += count;
        if (size_ >= High) paused_ = true;
        return BufferResult::stored;
    }

    // At most one nonblocking OS attempt. Sender cannot reenter this queue.
    template<class Sender>
    FlushResult flush(Sender&& send) noexcept {
        static_assert(std::is_nothrow_invocable_r_v<SendAttempt, Sender&,
                      const std::uint8_t*, std::size_t>, "sender must be noexcept");
        if (size_ == 0) return FlushResult::empty;
        const auto contiguous = (std::min)(size_, Capacity - head_);
        const auto result = send(bytes_.data() + head_, contiguous);
        if (result.state == SendState::progress) {
            if (result.count == 0 || result.count > contiguous || result.error != 0)
                return FlushResult::invalid;
            head_ = (head_ + result.count) % Capacity;
            size_ -= result.count;
            total_.fetch_sub(result.count, std::memory_order_relaxed);
            if (size_ <= Low) paused_ = false;
            return FlushResult::progress;
        }
        if (result.count != 0) return FlushResult::invalid;
        if (result.state == SendState::error) return FlushResult::error;
        if ((result.state != SendState::would_block && result.state != SendState::interrupted) ||
            result.error != 0) return FlushResult::invalid;
        return FlushResult::waiting;
    }

    void close() noexcept {
        if (closed_) return;
        closed_ = true;
        total_.fetch_sub(size_, std::memory_order_relaxed);
        size_ = 0;
        head_ = 0;
        paused_ = false;
    }
private:
    std::atomic<std::size_t>& total_;
    const std::size_t limit_;
    std::array<std::uint8_t, Capacity> bytes_{};
    std::size_t head_ = 0, size_ = 0;
    bool paused_ = false, closed_ = false;
};
} // namespace study_net
