#pragma once
#include "pacing.h"
#include <string>
#include <vector>

namespace bot {
struct Opponent {
    std::string name;
    std::string path;
    int inputIntervalTicks = Pacing{}.interval;
    std::string id;
    std::string iconPath;
    std::string portraitPath;
    std::string difficulty = "Normal";
    int thinkTicks = Pacing{}.think;
    int minPieceTicks = Pacing{}.minimum;
};

// Character entries can share a model. Legacy model scanning remains available.
// Asset/model paths are relative to the game resource working directory.
std::vector<Opponent> discover_opponents(const char* characters = "assets/opponents.cfg",
                                        const char* legacy = "model/bots.cfg");
int clamp_input_interval(int ticks);
}
