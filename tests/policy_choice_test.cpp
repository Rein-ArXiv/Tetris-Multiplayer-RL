#include "../bot/policy_choice.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <limits>

namespace {
int failures = 0;
void check(bool cond, const char* msg) {
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}
} // namespace

int main() {
    using bot::choose_finite_legal;
    int action = 0;

    {
        const float s[] = {-5.0f, -2.0f, -9.0f};
        const bool l[] = {true, true, true};
        action = 42;
        check(choose_finite_legal(s, l, 3, action), "finite negative true");
        check(action == 1, "finite negative picks largest");
    }
    {
        const float s[] = {1.0f, 1.0f, 0.0f};
        const bool l[] = {true, true, true};
        action = 42;
        check(choose_finite_legal(s, l, 3, action), "tie true");
        check(action == 0, "tie picks first");
    }
    {
        const float s[] = {10.0f, 20.0f, 5.0f};
        const bool l[] = {true, false, true};
        action = 42;
        check(choose_finite_legal(s, l, 3, action), "mask true");
        check(action == 0, "mask excludes illegal");
    }
    {
        const float s[] = {1.0f, std::numeric_limits<float>::quiet_NaN()};
        const bool l[] = {true, false};
        action = 7;
        check(!choose_finite_legal(s, l, 2, action), "NaN false");
        check(action == 7, "NaN unchanged");
    }
    {
        const float s[] = {1.0f, std::numeric_limits<float>::infinity()};
        const bool l[] = {true, false};
        action = 7;
        check(!choose_finite_legal(s, l, 2, action), "+inf false");
        check(action == 7, "+inf unchanged");
    }
    {
        const float s[] = {-std::numeric_limits<float>::infinity(), 1.0f};
        const bool l[] = {false, true};
        action = 7;
        check(!choose_finite_legal(s, l, 2, action), "-inf false");
        check(action == 7, "-inf unchanged");
    }
    {
        const float s[] = {1.0f, 2.0f};
        const bool l[] = {false, false};
        action = 9;
        check(!choose_finite_legal(s, l, 2, action), "empty mask false");
        check(action == 9, "empty mask unchanged");
    }
    {
        const float s[] = {1.0f};
        const bool l[] = {true};
        action = 9;
        check(!choose_finite_legal(nullptr, l, 1, action), "null scores false");
        check(!choose_finite_legal(s, nullptr, 1, action), "null legal false");
        check(action == 9, "null unchanged");
    }
    {
        const float s[] = {1.0f};
        const bool l[] = {true};
        action = 9;
        check(!choose_finite_legal(s, l, 0, action), "zero false");
        check(action == 9, "zero unchanged");
    }
    {
        const float s[] = {1.0f};
        const bool l[] = {true};
        action = 9;
        check(!choose_finite_legal(s, l, static_cast<std::size_t>(INT_MAX) + 1, action),
              "count>INT_MAX false");
        check(action == 9, "count>INT_MAX unchanged");
    }

    if (failures == 0) {
        std::printf("All tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
