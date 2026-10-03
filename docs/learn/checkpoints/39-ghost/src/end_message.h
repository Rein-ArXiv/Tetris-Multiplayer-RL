#pragma once
#include "simulation/round.h"
namespace study_ui {
inline const char* end_message(study_round::EndReason reason) noexcept {
    switch(reason){
    case study_round::EndReason::none: return "PLAYING";
    case study_round::EndReason::initial_spawn_blocked: return "GAME OVER: initial spawn blocked. Escape to exit.";
    case study_round::EndReason::spawn_blocked: return "GAME OVER: next spawn blocked after lock. Escape to exit.";
    }
    return "Unknown end reason";
}
} // namespace study_ui
