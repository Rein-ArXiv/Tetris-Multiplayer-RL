#pragma once
#include "simulation/pending_horizontal.h"
namespace study_input {
struct Intent { int horizontal; bool clockwise; };
class PendingControls {
public:
    void capture(bool left,bool right,bool up) noexcept {
        horizontal_.capture(left,right);
        clockwise_ = clockwise_ || up;
    }
    Intent consume() noexcept {
        Intent result{horizontal_.consume(),clockwise_};
        clockwise_ = false;
        return result;
    }
private:
    PendingHorizontal horizontal_;
    bool clockwise_ = false;
};
} // namespace study_input
