// core/key_edges.h
#ifndef CORE_KEY_EDGES_H
#define CORE_KEY_EDGES_H

#include <array>
#include <cstddef>

namespace input_detail {

// Per-key edge state for a single frame.
//   held_     : key is currently down (persists across frames)
//   pressed_  : a real (non-repeat) false->true transition happened
//   released_ : a true->false transition happened (cancel() also sets it)
//   cancelled_: cancel() was called this frame
// Count must be positive. No heap and no IO.
template <std::size_t Count>
class KeyEdges {
    static_assert(Count > 0, "KeyEdges requires Count > 0");

public:
    KeyEdges() noexcept = default;

    // Start a new frame: clears the per-frame edges and the cancel flag,
    // but keeps held_ so keys stay held across frames.
    void begin_frame() noexcept {
        for (std::size_t i = 0; i < Count; ++i) {
            pressed_[i] = false;
            released_[i] = false;
        }
        cancelled_ = false;
    }

    // Apply one key event; out-of-range keys are ignored.
    // A repeat never creates a new press (e.g. after focus regain), so
    // drop it instead of letting OS auto-repeat synthesize a press.
    void set(std::size_t key, bool down, bool repeat = false) noexcept {
        if (key >= Count) return;
        if (down && repeat) return;
        const bool was = held_[key];
        if (down && !was) pressed_[key] = true;
        if (!down && was) released_[key] = true;
        held_[key] = down;
    }

    bool down(std::size_t key) const noexcept { return key < Count ? held_[key] : false; }
    bool pressed(std::size_t key) const noexcept { return key < Count ? pressed_[key] : false; }
    bool released(std::size_t key) const noexcept { return key < Count ? released_[key] : false; }

    // Cancel this frame: held keys count as released, the same-frame press
    // is erased, and cancelled() stays true until the next begin_frame().
    // A later set() in the same frame is still accepted.
    void cancel() noexcept {
        for (std::size_t i = 0; i < Count; ++i) {
            released_[i] = released_[i] || held_[i];
            held_[i] = false;
            pressed_[i] = false;
        }
        cancelled_ = true;
    }

    bool cancelled() const noexcept { return cancelled_; }

    // Clear every piece of state.
    void reset() noexcept {
        for (std::size_t i = 0; i < Count; ++i) {
            held_[i] = false;
            pressed_[i] = false;
            released_[i] = false;
        }
        cancelled_ = false;
    }

private:
    std::array<bool, Count> held_{};
    std::array<bool, Count> pressed_{};
    std::array<bool, Count> released_{};
    bool cancelled_ = false;
};

}  // namespace input_detail

#endif  // CORE_KEY_EDGES_H
