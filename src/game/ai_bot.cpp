#include "ai_bot.h"
#include <iostream>
#include <fstream>
#include <random>
#include <cstring>

// AI 모델 유형 문자열 변환
std::string AIModelTypeToString(AIModelType type) {
    switch (type) {
        case AIModelType::DQN: return "DQN";
        case AIModelType::RAINBOW: return "Rainbow";
        case AIModelType::A3C: return "A3C";
        case AIModelType::PPO: return "PPO";
        case AIModelType::MUZERO: return "MuZero";
        default: return "Unknown";
    }
}

AIModelType StringToAIModelType(const std::string& str) {
    if (str == "DQN") return AIModelType::DQN;
    if (str == "Rainbow") return AIModelType::RAINBOW;
    if (str == "A3C") return AIModelType::A3C;
    if (str == "PPO") return AIModelType::PPO;
    if (str == "MuZero") return AIModelType::MUZERO;
    
    // 기본값
    return AIModelType::DQN;
}

// SafeTensor 봇 구현
SafeTensorBot::SafeTensorBot(AIModelType modelType, const std::string& modelPath)
    : modelType(modelType), modelPath(modelPath), modelHandle(nullptr) {
    
    // 모델 이름 설정
    modelName = AIModelTypeToString(modelType) + " Bot";
    
    // 실제 구현에서는 여기에 SafeTensor 모델 로드 코드가 들어갑니다.
    std::cout << "Loading AI model: " << modelName << " from " << modelPath << std::endl;
    
    // 모델 로드 시뮬레이션
    // 실제 구현에서는 Python 서비스와 통신하여 모델을 로드합니다.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 더미 모델 핸들 생성
    modelHandle = new char[1];
}

SafeTensorBot::~SafeTensorBot() {
    // 모델 리소스 정리
    if (modelHandle) {
        delete[] static_cast<char*>(modelHandle);
        modelHandle = nullptr;
    }
}

InputAction SafeTensorBot::DecideAction(const Game& game) {
    // 게임 상태를 모델 입력으로 변환
    std::vector<float> modelInput = GameStateToModelInput(game);
    
    // 실제 구현에서는 여기에 모델 추론 코드가 들어갑니다.
    // Python 서비스로 입력을 전송하고 예측 결과를 받아옵니다.
    
    // 더미 모델 출력 생성 (실제 구현에서는 실제 모델 출력 사용)
    std::vector<float> modelOutput(5, 0.0f); // 5가지 액션에 대한 확률
    
    // 여기서는 간단한 랜덤 결정 사용 (더미 구현)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0, 1);
    
    for (size_t i = 0; i < modelOutput.size(); ++i) {
        modelOutput[i] = dist(gen);
    }
    
    // 모델의 결정을 게임 액션으로 변환
    return ModelOutputToAction(modelOutput);
}

std::vector<float> SafeTensorBot::GameStateToModelInput(const Game& game) {
    // 게임 상태를 모델 입력 형식으로 변환
    std::vector<float> input;
    
    // 게임 보드 상태 변환
    const auto& boardState = game.GetBoardState();
    for (const auto& row : boardState) {
        for (int cell : row) {
            // 셀 값을 0 또는 1로 변환 (0=빈칸, 1=블록)
            input.push_back(cell > 0 ? 1.0f : 0.0f);
        }
    }
    
    // 현재 블록 정보 추가
    const Block& currentBlock = game.GetCurrentBlock();
    int blockType = static_cast<int>(currentBlock.type);
    
    // 원-핫 인코딩으로 블록 타입 표현
    for (int i = 1; i <= 7; ++i) {
        input.push_back(i == blockType ? 1.0f : 0.0f);
    }
    
    // 다음 블록 정보 추가
    const Block& nextBlock = game.GetNextBlock();
    int nextBlockType = static_cast<int>(nextBlock.type);
    
    // 원-핫 인코딩으로 다음 블록 타입 표현
    for (int i = 1; i <= 7; ++i) {
        input.push_back(i == nextBlockType ? 1.0f : 0.0f);
    }
    
    return input;
}

InputAction SafeTensorBot::ModelOutputToAction(const std::vector<float>& output) {
    // 가장 높은 확률의 액션 선택
    auto maxIt = std::max_element(output.begin(), output.end());
    int actionIdx = std::distance(output.begin(), maxIt);
    
    // 인덱스를 InputAction으로 변환
    switch (actionIdx) {
        case 0: return InputAction::MOVE_LEFT;
        case 1: return InputAction::MOVE_RIGHT;
        case 2: return InputAction::ROTATE;
        case 3: return InputAction::MOVE_DOWN;
        case 4: return InputAction::HARD_DROP;
        default: return InputAction::NONE;
    }
}

// AI 봇 매니저 구현
std::unique_ptr<AIBotManager> AIBotManager::instance = nullptr;

AIBotManager& AIBotManager::Instance() {
    if (!instance) {
        instance = std::unique_ptr<AIBotManager>(new AIBotManager());
    }
    return *instance;
}

AIBotManager::AIBotManager() {
    // 기본 모델 경로 설정
    modelBasePath = "./models/";
    
    // 사용 가능한 모델 초기화
    InitializeModelPaths();
}

AIBotManager::~AIBotManager() {
    // 정리 작업
}

void AIBotManager::InitializeModelPaths() {
    // 모든 지원 모델 등록
    availableModels.push_back({AIModelType::DQN, modelBasePath + "dqn_tetris.safetensors"});
    availableModels.push_back({AIModelType::RAINBOW, modelBasePath + "rainbow_tetris.safetensors"});
    availableModels.push_back({AIModelType::A3C, modelBasePath + "a3c_tetris.safetensors"});
    availableModels.push_back({AIModelType::PPO, modelBasePath + "ppo_tetris.safetensors"});
    availableModels.push_back({AIModelType::MUZERO, modelBasePath + "muzero_tetris.safetensors"});
}

std::shared_ptr<AIBot> AIBotManager::CreateBot(AIModelType modelType) {
    // 요청된 모델 유형에 맞는 경로 찾기
    std::string modelPath;
    for (const auto& model : availableModels) {
        if (model.first == modelType) {
            modelPath = model.second;
            break;
        }
    }
    
    if (modelPath.empty()) {
        std::cerr << "요청된 모델 유형을 찾을 수 없습니다: " << AIModelTypeToString(modelType) << std::endl;
        return nullptr;
    }
    
    // 모델 파일 존재 여부 확인
    std::ifstream file(modelPath);
    if (!file.good()) {
        std::cerr << "모델 파일을 찾을 수 없습니다: " << modelPath << std::endl;
        std::cerr << "더미 모델을 사용합니다." << std::endl;
        // 실제 구현에서는 오류 처리 또는 기본 모델 사용
    }
    
    // 새 AI 봇 생성
    return std::make_shared<SafeTensorBot>(modelType, modelPath);
}

std::vector<std::string> AIBotManager::GetAvailableModels() const {
    std::vector<std::string> models;
    for (const auto& model : availableModels) {
        models.push_back(AIModelTypeToString(model.first));
    }
    return models;
}
