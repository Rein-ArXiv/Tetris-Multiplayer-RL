#pragma once

namespace study_input {

// One simulation tick's semantic input.
// horizontal: -1 = left, 0 = neutral, +1 = right.
struct Intent {
    int horizontal;
    bool clockwise;
    bool soft_drop = false;
    bool hard_drop = false;
};

} // namespace study_input
