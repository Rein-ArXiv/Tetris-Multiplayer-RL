#include "player.h"

Player::Player(int id, const std::string& name, PlayerType type) 
    : id(id), name(name), type(type), lastAction(InputAction::NONE) {
    // 플레이어 초기화
    game.Initialize();
}