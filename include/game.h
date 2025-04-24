#ifndef GAME_H
#define GAME_H

#include <ncurses.h>
#include <memory>
#include "grid.h"
#include "block.h"

// 게임 상태 정의
enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER
};

// 키보드 입력 정의
enum class InputAction {
    NONE,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_DOWN,
    ROTATE,
    HARD_DROP,
    HOLD,
    PAUSE,
    QUIT
};

class Game {
public:
    Game();
    ~Game();
    
    // 게임 루프 관련 함수
    void Initialize();
    void Draw(WINDOW* win);
    InputAction HandleInput(int key);
    void Update(double deltaTime);
    void ProcessAction(InputAction action);
    
    // 게임 상태 관리
    bool IsGameOver() const { return state == GameState::GAME_OVER; }
    void Reset();
    
    // 블록 조작 함수
    void MoveBlockLeft();
    void MoveBlockRight();
    void MoveBlockDown();
    void RotateBlock();
    void HardDropBlock();
    void HoldBlock();
    
    // 게임 상태 확인 및 업데이트
    bool BlockFits(const Block& block) const;
    void LockBlock();
    void UpdateGhostBlock();
    
    // 게임 데이터 접근
    std::vector<std::vector<int>> GetBoardState() const;
    Block GetCurrentBlock() const { return currentBlock; }
    Block GetNextBlock() const { return nextBlock; }
    int GetScore() const { return score; }
    int GetLevel() const { return level; }
    int GetLinesCleared() const { return linesCleared; }
    int GetLastClearedLines() const { return lastClearedLines; }
    
    // 멀티플레이어용 함수
    void AddGarbageLines(int count) { grid.AddGarbageLines(count); }

    // AI 확인용 함수
    const Grid& GetGrid() const { return grid; }
    
private:
    // 게임 블록 관리
    Block currentBlock;
    Block nextBlock;
    Block ghostBlock;
    Block heldBlock;
    bool hasHeld;
    
    // 게임 보드 및 상태
    Grid grid;
    GameState state;
    
    // 점수 및 난이도
    int score;
    int level;
    int linesCleared;
    int lastClearedLines;
    double dropInterval;
    double dropTimer;
    
    // 게임 내부 함수
    Block GetRandomBlock();
    void UpdateScore(int linesCleared);
    void UpdateLevel();
    void UpdateDropInterval();
};

#endif // GAME_H