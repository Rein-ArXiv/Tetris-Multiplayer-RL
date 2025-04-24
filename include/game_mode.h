#ifndef GAME_MODE_H
#define GAME_MODE_H

#include <string>

// 게임 모드 열거형
enum class GameMode {
    SINGLE_PLAYER,      // 1인 플레이
    SINGLE_VS_AI,       // 1인 vs AI
    MULTIPLAYER,        // 멀티플레이어 (인간만)
    MULTIPLAYER_WITH_AI // 멀티플레이어 (AI 포함)
};

// 게임 모드 문자열 변환 함수
std::string GameModeToString(GameMode mode);
GameMode StringToGameMode(const std::string& str);

#endif // GAME_MODE_H
