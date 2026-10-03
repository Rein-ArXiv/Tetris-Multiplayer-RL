#pragma once
#include "src/kicks_example.h"
namespace kicks_cases {
struct Case : kicks_example::Example {
    int expected_index;
    study_piece::Origin expected_origin;
};
inline Case make(int number) {
    // Independent expected outcomes of the fixed demonstration inputs.
    constexpr int indices[]{1,5,6,1,2,-1,0};
    constexpr study_piece::Origin origins[]{{5,0},{17,3},{16,3},{5,2},{5,4},{5,3},{5,3}};
    return {kicks_example::make(number),indices[number],origins[number]};
}
} // namespace kicks_cases
