#pragma once
#include "simulation/movement.h"

namespace study_input {
// Coalesce edges until a tick consumes them; this is not an event queue.
class PendingHorizontal {
public:
    void capture(bool left_pressed, bool right_pressed) noexcept {
        left_ = left_ || left_pressed;
        right_ = right_ || right_pressed;
    }
    int consume() noexcept {
        const int direction = study_movement::horizontal_intent(left_, right_);
        left_ = right_ = false;
        return direction;
    }
private:
    bool left_ = false;
    bool right_ = false;
};
} // namespace study_input
