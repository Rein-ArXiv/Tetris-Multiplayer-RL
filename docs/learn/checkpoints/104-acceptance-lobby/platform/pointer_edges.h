#pragma once
// pointer_edges.h - per-frame pointer edge state from trusted native events (C++17).
#include <optional>

namespace study_pointer {

struct Point { int x = 0; int y = 0; };

struct Frame {
    std::optional<Point> position;
    std::optional<Point> press;
    bool down = false;
    bool cancelled = false;
};

// Collects native left-button transitions in window coordinates, independently
// from drawable pixels. This bounded state machine retains one press per frame.
class Edges final {
public:
    // Start a new frame: forget the previous press and cancellation, keep the
    // latest position and held state (a button may span frames).
    void begin_frame() noexcept {
        press_.reset();
        cancelled_ = false;
    }

    // Record the newest pointer position. Does not change the held state.
    void move(Point p) noexcept { position_ = p; }

    // Record a button transition. The first false->true edge of the frame wins
    // the press; later down/up/down cycles do not overwrite it.
    void set(bool down, Point p) noexcept {
        position_ = p;
        if (down && !down_ && !press_) press_ = p;
        down_ = down;
    }

    // Drop all held input for this frame and mark it cancelled. Later events may
    // still update the internal state; snapshot() keeps hiding them until the
    // next begin_frame().
    void cancel() noexcept {
        position_.reset();
        press_.reset();
        down_ = false;
        cancelled_ = true;
    }

    // Return the frame as seen by consumers. While cancelled this frame, the
    // snapshot exposes no coordinates and no held/pressed state.
    Frame snapshot() const noexcept {
        if (cancelled_) return Frame{std::nullopt, std::nullopt, false, true};
        return Frame{position_, press_, down_, false};
    }

    // Forget everything, including cancellation.
    void reset() noexcept {
        position_.reset();
        press_.reset();
        down_ = false;
        cancelled_ = false;
    }

private:
    std::optional<Point> position_;
    std::optional<Point> press_;
    bool down_ = false;
    bool cancelled_ = false;
};

}  // namespace study_pointer
