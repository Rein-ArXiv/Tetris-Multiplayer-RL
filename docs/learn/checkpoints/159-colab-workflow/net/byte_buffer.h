#ifndef STUDY_NET_BYTE_BUFFER_H
#define STUDY_NET_BYTE_BUFFER_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace study_net {

// Fixed-capacity byte accumulator. It is deliberately single-threaded and
// unframed: it stores a raw byte stream with no string interpretation and no
// dynamic allocation.
template <std::size_t Capacity>
class ByteBuffer {
public:
    ByteBuffer() noexcept : bytes_{}, used_(0) {}

    // Append `count` readable bytes from the borrowed buffer `data`.
    // On failure, contents and size remain unchanged; no thread atomicity.
    // Copy only when the whole request fits. Empty append accepts null.
    bool append(const std::uint8_t* data, std::size_t count) noexcept {
        if (count == 0) {
            return true;  // explicit empty append, null is fine
        }
        if (data == nullptr) {
            return false;  // non-empty append needs a readable buffer
        }
        if (count > Capacity - used_) {
            return false;  // would overflow; reject before copying anything
        }
        for (std::size_t index = 0; index < count; ++index) {
            bytes_[used_ + index] = data[index];
        }
        used_ += count;
        return true;
    }

    // Remove a consumed prefix, retaining the suffix in order.
    // Reject first so subtraction cannot underflow. Overlap is intentional:
    // forward copies write only at or before the element being read.
    bool consume_front(std::size_t count) noexcept {
        if (count > used_) return false;
        const std::size_t left = used_ - count;
        for (std::size_t index = 0; index < left; ++index) {
            bytes_[index] = bytes_[count + index];
        }
        used_ = left;
        return true;
    }

    const std::uint8_t* data() const noexcept { return bytes_.data(); }
    std::size_t size() const noexcept { return used_; }
    std::size_t remaining() const noexcept { return Capacity - used_; }

private:
    std::array<std::uint8_t, Capacity> bytes_;
    std::size_t used_;
};

}  // namespace study_net

#endif  // STUDY_NET_BYTE_BUFFER_H
