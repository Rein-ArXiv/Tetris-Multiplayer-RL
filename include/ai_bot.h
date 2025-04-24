#ifndef AI_BOT_H
#define AI_BOT_H

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <map>
#include "game.h"

// 지원하는 AI 모델 유형
enum class AIModelType {
    DQN,
    RAINBOW,
    A3C,
    PPO,
    MUZERO
};

// AI 모델 유형을 문자열로 변환
std::string AIModelTypeToString(AIModelType type);
// 문자열을 AI 모델 유형으로 변환
AIModelType StringToAIModelType(const std::string& str);

// AI 봇 클래스 (추상 인터페이스)
class AIBot {
public:
    virtual ~AIBot() = default;
    
    // 게임 상태를 관찰하고 다음 액션 결정
    virtual InputAction DecideAction(const Game& game) = 0;
    
    // 모델 타입 반환
    virtual AIModelType GetModelType() const = 0;
    
    // 모델 이름 반환
    virtual std::string GetModelName() const = 0;
};

class DummyAIBot : public AIBot {
public:
    DummyAIBot(AIModelType modelType);
    ~DummyAIBot() override = default;
    
    // 게임 상태를 관찰하고 다음 액션 결정
    InputAction DecideAction(const Game& game) override;
    
    // 모델 타입 반환
    AIModelType GetModelType() const override { return modelType; }
    
    // 모델 이름 반환
    std::string GetModelName() const override { return AIModelTypeToString(modelType) + " Bot"; }
    
private:
    AIModelType modelType;
    std::mt19937 rng;
    
    // 모델 타입별 전략
    std::vector<float> GetActionProbabilities(const Game& game);
    
    // 전략별 히트맵 (각 모델 타입마다 다른 행동 패턴)
    std::map<AIModelType, std::vector<float>> strategyHeatmap = {
        {AIModelType::DQN,     {0.3f, 0.3f, 0.2f, 0.1f, 0.1f}},  // 좌/우 이동 선호
        {AIModelType::RAINBOW, {0.2f, 0.2f, 0.4f, 0.1f, 0.1f}},  // 회전 선호
        {AIModelType::A3C,     {0.2f, 0.2f, 0.2f, 0.2f, 0.2f}},  // 균등한 선택
        {AIModelType::PPO,     {0.1f, 0.1f, 0.3f, 0.1f, 0.4f}},  // 하드 드롭 선호
        {AIModelType::MUZERO,  {0.25f, 0.25f, 0.3f, 0.1f, 0.1f}} // 좌/우 이동 + 회전 선호
    };
};
    

// SafeTensor 기반 AI 봇 (실제 구현)
class SafeTensorBot : public AIBot {
public:
    SafeTensorBot(AIModelType modelType, const std::string& modelPath);
    ~SafeTensorBot();
    
    // 게임 상태를 관찰하고 다음 액션 결정
    InputAction DecideAction(const Game& game) override;
    
    // 모델 타입 반환
    AIModelType GetModelType() const override { return modelType; }
    
    // 모델 이름 반환
    std::string GetModelName() const override { return modelName; }
    
private:
    AIModelType modelType;
    std::string modelPath;
    std::string modelName;
    
    // 게임 상태를 AI 모델 입력으로 변환
    std::vector<float> GameStateToModelInput(const Game& game);
    
    // 모델 출력을 게임 액션으로 변환
    InputAction ModelOutputToAction(const std::vector<float>& output);
    
    // SafeTensor 모델 로드 및 추론 기능
    // (실제 구현에서는 Python 서비스와 통신)
    void* modelHandle;  // 모델 핸들 (실제로는 더 구체적인 유형이 필요)
};

// AI 봇 매니저 (봇 생성 및 관리)
class AIBotManager {
public:
    static AIBotManager& Instance();
    
    // 더미 봇 생성
    std::shared_ptr<AIBot> CreateDummyBot(AIModelType modelType);

    // 특정 모델 유형의 AI 봇 생성
    std::shared_ptr<AIBot> CreateBot(AIModelType modelType);
    
    // 모든 지원 모델 목록 가져오기
    std::vector<std::string> GetAvailableModels() const;
    
private:
    AIBotManager();
    ~AIBotManager();
    
    // 싱글톤 인스턴스
    static std::unique_ptr<AIBotManager> instance;
    
    // 모델 경로 정보
    std::string modelBasePath;
    std::vector<std::pair<AIModelType, std::string>> availableModels;
    
    // 모델 경로 초기화
    void InitializeModelPaths();
};

#endif // AI_BOT_H