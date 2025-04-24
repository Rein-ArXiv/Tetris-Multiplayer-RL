#include "game_mode.h"

std::string GameModeToString(GameMode mode) {
    switch (mode) {
        case GameMode::SINGLE_PLAYER: return "Single Player";
        case GameMode::SINGLE_VS_AI: return "Single Player vs AI";
        case GameMode::MULTIPLAYER: return "Multiplayer";
        case GameMode::MULTIPLAYER_WITH_AI: return "Multiplayer with AI";
        default: return "Unknown";
    }
}

GameMode StringToGameMode(const std::string& str) {
    if (str == "Single Player") return GameMode::SINGLE_PLAYER;
    if (str == "Single Player vs AI") return GameMode::SINGLE_VS_AI;
    if (str == "Multiplayer") return GameMode::MULTIPLAYER;
    if (str == "Multiplayer with AI") return GameMode::MULTIPLAYER_WITH_AI;
    
    // 기본값
    return GameMode::SINGLE_PLAYER;
}