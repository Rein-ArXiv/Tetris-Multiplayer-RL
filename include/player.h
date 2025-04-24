#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <memory>
#include "game.h"

// 플레이어 타입 정의
enum class PlayerType {
    HUMAN,
    BOT,
    REMOTE
};

class Player {
public:
    Player(int id, const std::string& name, PlayerType type = PlayerType::HUMAN);
    
    // 플레이어 정보
    int GetId() const { return id; }
    std::string GetName() const { return name; }
    PlayerType GetType() const { return type; }
    
    // 게임 관련
    Game& GetGame() { return game; }
    const Game& GetGame() const { return game; }
    
    // 네트워크 관련
    void SetLastInputAction(InputAction action) { lastAction = action; }
    InputAction GetLastInputAction() const { return lastAction; }
    
private:
    int id;
    std::string name;
    PlayerType type;
    Game game;
    InputAction lastAction;
};

#endif // PLAYER_H