#include "../bot/run_status.h"

#include <stdexcept>
void require(bool ok){if(!ok)throw std::runtime_error("run status contract");}
#include <iostream>

using namespace bot;

static void test_practice_starts_clean() {
    RunStatus s;
    s.reset(Mode::practice);
    require(s.fault() == Fault::none);
    require(!s.degraded());
    require(!s.reward_eligible());
}

static void test_reward_starts_clean() {
    RunStatus s;
    s.reset(Mode::reward);
    require(s.fault() == Fault::none);
    require(!s.degraded());
    require(s.reward_eligible());
}

static void test_first_fault_is_preserved() {
    RunStatus s;
    s.reset(Mode::reward);
    s.observe(Fault::policy_failed);
    require(s.fault() == Fault::policy_failed);
    require(s.degraded());
    require(!s.reward_eligible());

    s.observe(Fault::invalid_target);
    s.observe(Fault::invalid_target);
    require(s.fault() == Fault::policy_failed);
}

static void test_none_never_restores_eligibility() {
    RunStatus s;
    s.reset(Mode::reward);
    s.observe(Fault::invalid_target);
    require(!s.reward_eligible());

    s.observe(Fault::none);
    require(s.fault() == Fault::invalid_target);
    require(s.degraded());
    require(!s.reward_eligible());
}

static void test_reset_clears_state() {
    RunStatus s;
    s.reset(Mode::reward);
    s.observe(Fault::invalid_target);
    require(s.degraded());

    s.reset(Mode::practice);
    require(s.fault() == Fault::none);
    require(!s.degraded());
    require(!s.reward_eligible());

    s.reset(Mode::reward);
    require(s.fault() == Fault::none);
    require(!s.degraded());
    require(s.reward_eligible());
}

static void test_clean_observations() {
    RunStatus s;
    s.reset(Mode::reward);
    s.observe(Fault::none);
    s.observe(Fault::none);
    require(s.fault() == Fault::none);
    require(!s.degraded());
    require(s.reward_eligible());
}

static void test_fault_blocks_practice_too() {
    RunStatus s;
    s.reset(Mode::practice);
    s.observe(Fault::policy_failed);
    require(s.fault() == Fault::policy_failed);
    require(s.degraded());
    require(!s.reward_eligible());
}

int main() {
    test_practice_starts_clean();
    test_reward_starts_clean();
    test_first_fault_is_preserved();
    test_none_never_restores_eligibility();
    test_reset_clears_state();
    test_clean_observations();
    test_fault_blocks_practice_too();

    std::cout << "All RunStatus tests passed\n";
    return 0;
}
