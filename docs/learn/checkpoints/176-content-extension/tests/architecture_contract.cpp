// architecture_contract.cpp
// Standalone C++17 architecture-contract exercises for the study_match library.
// Includes bot/policy_match.h and only standard headers.
// No SDL, OS, ONNX, or database dependencies. Library code is not modified.
#include "bot/policy_match.h"
#include "content/characters.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// require() throws std::runtime_error instead of using assert(), so the checks
// stay active even when NDEBUG removes assert().
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("architecture contract violation: " + message);
    }
}

study_characters::Appearance makeAppearance(std::string name) {
    return study_characters::Appearance{std::move(name), "icon", "portrait", "normal"};
}

// Slow fixture pacing keeps a handful of fixture inputs from finishing the
// match; every test still re-checks finished() before each tick.
study_characters::Character makeCharacter(const std::string& id,
                                          const study_characters::Appearance& appearance) {
    return study_characters::Character{
        id,
        appearance,
        study_characters::Behavior{"policy/model", bot::Pacing{6, 2, 6}}};
}

// Deterministic picker: always take the first legal action.
bool legalFirstPicker(const study_python::Session& observed, int& action) {
    const auto legal = observed.legal_actions();
    if (legal.empty()) return false;
    action = legal.front();
    return true;
}

std::vector<std::uint8_t> fixtureInputs() {
    return {study_input::rotate, study_input::left, study_input::rotate,
            study_input::right, study_input::left, study_input::rotate};
}

// 1) Appearance is not behavior: two characters that differ only in
//    Appearance must be indistinguishable to the match given the same seed,
//    the same inputs and the same deterministic legal-first picker.
void testAppearanceOnlyChangeIsBehaviorallyInert() {
    const std::uint64_t seed = 0x5EEDULL;
    const study_characters::Character styled = makeCharacter("id-175", makeAppearance("Alpha"));
    const study_characters::Character restyled = makeCharacter("id-175", makeAppearance("Beta"));

    require(styled.id == restyled.id, "fixtures must share identity");
    require(styled.behavior.model == restyled.behavior.model, "fixtures must share behavior.model");
    require(styled.appearance.name != restyled.appearance.name,
            "fixtures must differ in Appearance for this boundary test");

    study_match::Match a(seed, styled, bot::Mode::reward);
    study_match::Match b(seed, restyled, bot::Mode::reward);
    const auto inputs = fixtureInputs();
    require(!inputs.empty(), "fixture requires inputs");

    bool decisionObserved = false;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        require(!a.finished() && !b.finished(),
                "fixture pacing must keep both matches unfinished before input " + std::to_string(i));
        require(study_input::valid(inputs[i]), "fixture masks must be valid");

        const study_match::Frame fa = a.tick(inputs[i], legalFirstPicker, true);
        const study_match::Frame fb = b.tick(inputs[i], legalFirstPicker, true);
        decisionObserved = decisionObserved || fa.bot.decision_attempted;

        require(a.human().state_bytes() == b.human().state_bytes(),
                "human state must not depend on Appearance");
        require(a.enemy().state_bytes() == b.enemy().state_bytes(),
                "enemy state must not depend on Appearance");
        require(fa.bot.mask == fb.bot.mask, "emitted bot mask must not depend on Appearance");
        require(fa.source == fb.source, "decision source must not depend on Appearance");
    }
    require(decisionObserved, "appearance fixture must exercise a policy decision");
}

// 2) A picker that declines is recorded as a policy failure and strips reward
//    eligibility; the failed decision must not be silently rewarded.
void testFailedPickerRecordsPolicyFailure() {
    const study_characters::Character character = makeCharacter("id-fail", makeAppearance("Fail"));
    study_match::Match match(0x1111ULL, character, bot::Mode::reward);
    require(!match.finished(), "fixture must start unfinished");
    require(match.status().reward_eligible(), "a fresh match must be reward eligible");

    bool pickerCalled = false;
    auto decliningPicker = [&](const study_python::Session&, int&) -> bool {
        pickerCalled = true;
        return false; // no decision
    };

    bool sawUnavailable = false;
    for (int i = 0; i < 12 && !pickerCalled; ++i) {
        require(!match.finished(), "match must remain unfinished while awaiting a decision");
        const study_match::Frame frame = match.tick(study_input::rotate, decliningPicker, false);
        if (frame.source == study_match::Source::unavailable) sawUnavailable = true;
    }

    require(pickerCalled, "the picker must have been consulted");
    require(sawUnavailable, "no-fallback decline must be reported as unavailable");
    require(!match.status().reward_eligible(),
            "failed picker must record a policy failure and revoke reward eligibility");
}

// 3) An invalid input mask (255) must throw before any internal game state is
//    applied and before the policy is consulted.
void testInvalidMaskThrowsBeforeStateOrPolicy() {
    const study_characters::Character character = makeCharacter("id-mask", makeAppearance("Mask"));
    study_match::Match match(0x2222ULL, character, bot::Mode::reward);
    require(!match.finished(), "fixture must start unfinished");

    const auto humanBefore = match.human().state_bytes();
    const auto enemyBefore = match.enemy().state_bytes();

    bool policyCalled = false;
    auto spyPicker = [&](const study_python::Session&, int&) -> bool {
        policyCalled = true;
        return true;
    };

    bool threw = false;
    try {
        match.tick(255, spyPicker, true);
    } catch (const std::exception&) {
        threw = true;
    }

    require(threw, "invalid input mask 255 must throw");
    require(!policyCalled, "policy must not be called when the mask is invalid");
    require(match.human().state_bytes() == humanBefore, "human state must be unchanged");
    require(match.enemy().state_bytes() == enemyBefore, "enemy state must be unchanged");
    require(!match.finished(), "an invalid mask must not finish the match");
}

// 4) An exception thrown by a policy is absorbed by Match, but arbitrary
//    external side effects already performed by that callback are not undone
//    by the candidate state mechanism. Match handles this failure and commits its fault state.
struct injectedPolicyError : std::runtime_error {
    explicit injectedPolicyError(const std::string& what) : std::runtime_error(what) {}
};

void testHandledPolicyFailureRetainsExternalEffect() {
    const study_characters::Character character = makeCharacter("id-extern", makeAppearance("Extern"));
    study_match::Match match(0x3333ULL, character, bot::Mode::reward);
    require(!match.finished(), "fixture must start unfinished");
    require(match.status().reward_eligible(), "a fresh match must be reward eligible");

    int externalCounter = 0;
    auto throwingPicker = [&](const study_python::Session&, int&) -> bool {
        ++externalCounter; // external side effect owned by the caller
        throw injectedPolicyError("injected policy failure");
    };

    study_match::Frame frame{};
    bool consulted = false;
    for (int i = 0; i < 12 && !consulted; ++i) {
        require(!match.finished(), "match must remain unfinished while awaiting a decision");
        frame = match.tick(study_input::rotate, throwingPicker, true);
        consulted = externalCounter > 0;
    }

    require(consulted, "the picker must have been consulted");
    require(externalCounter == 1, "the external counter increment must remain visible");
    require(frame.source == study_match::Source::fallback,
            "Match must handle the injected runtime_error and fall back");
    require(!match.status().reward_eligible(),
            "a handled policy fault must still revoke reward eligibility");
    require(!match.finished(), "the injected failure must not finish the match");
}


// Driver propagates policy errors: internal candidates disappear, external effects remain.
void testDriverRollbackDoesNotUndoCallback() {
    study_python::Session live(77);
    study_paced::Driver driver({1, 0, 1});
    const auto before = live.state_bytes();
    const auto age = driver.age_for_gates();
    int calls = 0;
    bool threw = false;
    try {
        driver.tick(live, [&](const study_python::Session&, int&) -> bool {
            ++calls;
            throw std::runtime_error("injected external policy failure");
        });
    } catch (const std::runtime_error&) { threw = true; }
    require(threw && calls == 1, "policy effect must remain after propagation");
    require(live.state_bytes() == before && driver.age_for_gates() == age,
            "Driver and Session candidates must not be committed");
}

} // namespace

int main() {
    require(study_character_art::catalog_valid(), "visual catalog remains valid beside bot profiles");
    struct Case {
        const char* name;
        void (*run)();
    };
    const Case cases[] = {
        {"Driver rollback excludes callback effects", &testDriverRollbackDoesNotUndoCallback},
        {"appearance-only change is behaviorally inert", &testAppearanceOnlyChangeIsBehaviorallyInert},
        {"failed picker records policy failure", &testFailedPickerRecordsPolicyFailure},
        {"invalid mask throws before state or policy", &testInvalidMaskThrowsBeforeStateOrPolicy},
        {"handled policy failure retains external effect", &testHandledPolicyFailureRetainsExternalEffect},
    };

    int failures = 0;
    for (const Case& test : cases) {
        try {
            test.run();
            std::cout << "[pass] " << test.name << std::endl;
        } catch (const std::exception& error) {
            ++failures;
            std::cout << "[fail] " << test.name << ": " << error.what() << std::endl;
        }
    }

    if (failures != 0) {
        std::cout << failures << " contract test(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all architecture contract tests passed" << std::endl;
    return 0;
}