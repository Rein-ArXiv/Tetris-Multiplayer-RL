#include "ai_player.h"
#include <iostream>

AIPlayer::AIPlayer(int id, const std::string& name, AIModelType modelType)
    : Player(id, name, PlayerType::BOT), modelType(modelType),
      aiRunning(false), actionDelay(300) { // 300ms 딜레이 (초당 약 3액션)
    
    // AI 봇 생성
    bot = AIBotManager::Instance().CreateBot(modelType);
    
    if (!bot) {
        std::cerr << "AI 봇 생성 실패: " << name << std::endl;
    } else {
        std::cout << "AI 봇 생성 성공: " << name << " (모델: " << bot->GetModelName() << ")" << std::endl;
    }
}

AIPlayer::~AIPlayer() {
    // AI 중지
    StopAI();
}

std::string AIPlayer::GetModelName() const {
    return bot ? bot->GetModelName() : "Unknown";
}

void AIPlayer::StartAI() {
    if (aiRunning || !bot) {
        return;
    }
    
    aiRunning = true;
    aiThread = std::thread(&AIPlayer::AIThreadFunction, this);
}

void AIPlayer::StopAI() {
    if (!aiRunning) {
        return;
    }
    
    aiRunning = false;
    
    if (aiThread.joinable()) {
        aiThread.join();
    }
}

void AIPlayer::AIThreadFunction() {
    while (aiRunning) {
        // 게임이 진행 중이면 AI가 액션 결정
        if (!game.IsGameOver()) {
            // AI가 다음 액션 결정
            InputAction action = bot->DecideAction(game);
            
            // 결정된 액션 실행
            game.ProcessAction(action);
            
            // 마지막 입력 액션 업데이트
            SetLastInputAction(action);
        }
        
        // 액션 사이에 지연 추가 (너무 빠른 속도 방지)
        std::this_thread::sleep_for(actionDelay);
    }
}