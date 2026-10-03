#ifndef TETRIS_STUDY_TEXT_QUEUE_H
#define TETRIS_STUDY_TEXT_QUEUE_H
#include <array>
#include <cstddef>

// Single-threaded FIFO. One slot stays unused to distinguish full from empty.
template<std::size_t Slots>
class AsciiQueue {
    static_assert(Slots >= 2, "A ring needs a sentinel slot and a data slot");
public:
    bool push(unsigned char value) noexcept
    {
        if (value == 0 || value >= 128) return false; // 0 is reserved for empty.
        const auto next = (tail_ + 1) % Slots;
        if (next == head_) return false; // Drop newest; preserve queued input.
        data_[tail_] = static_cast<char>(value);
        tail_ = next;
        return true;
    }
    char pop() noexcept
    {
        if (head_ == tail_) return 0;
        const char value = data_[head_];
        head_ = (head_ + 1) % Slots;
        return value;
    }
private:
    std::array<char, Slots> data_{};
    std::size_t head_ = 0; // Next read position.
    std::size_t tail_ = 0; // Next write position.
};
#endif
