#ifndef AI_PLAYER_H
#define AI_PLAYER_H

#include <memory>
#include <thread>
#include <atomic>
#include "player.h"
#include "ai_bot.h"

// AI 플레이어 클래스 (Player 클래스 확장)
class AIPlayer : public Player {
public:
    AIPlayer(int id, const std::string& name, AIModelType modelType);
    ~AIPlayer();
    
    // AI 봇 관련
    AIModelType GetModelType() const { return modelType; }
    std::string GetModelName() const;
    void StartAI();
    void StopAI();
    
private:
    // AI 관련 멤버
    AIModelType modelType;
    std::shared_ptr<AIBot> bot;
    std::thread aiThread;
    std::atomic<bool> aiRunning;
    
    // AI 처리 스레드 함수
    void AIThreadFunction();
    
    // 제한된 액션 속도 (너무 빠르지 않게)
    std::chrono::milliseconds actionDelay;
};

#endif // AI_PLAYER_H