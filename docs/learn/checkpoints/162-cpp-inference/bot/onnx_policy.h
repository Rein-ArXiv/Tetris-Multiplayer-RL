#pragma once
#include "simulation/action_space.h"
#include <array>
#include <memory>
#include <string>

namespace study_python { class Session; }
namespace study_inference {
struct Prediction {
    std::array<float, study_actions::kCount> logits{};
    float value = 0;
    int action = -1;
};
// One owner serializes load, infer and destruction. Predictions own copied values.
class Policy {
public:
    Policy();
    ~Policy();
    Policy(const Policy&) = delete;
    Policy& operator=(const Policy&) = delete;
    bool load(const std::string& utf8_path, std::string& error);
    bool infer(const study_python::Session& state, Prediction& result, std::string& error);
    bool loaded() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
