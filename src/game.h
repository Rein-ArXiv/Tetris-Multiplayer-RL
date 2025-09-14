#pragma once
#include "grid.h"
#include "blocks.cpp"
#include "menu.h"
#include "../core/rng.h"         // [NET] 세션 시드로 초기화되는 결정론 RNG
#include "../core/input.h"       // [NET] 틱 단위 입력 비트마스크
#include "../core/constants.h"   // [NET] 틱 속도(동기 단위)

class Game{
public:
    // [NET] seed는 세션 합의(핸드셰이크)로 받은 동일 값이어야 합니다.
    Game(uint64_t seed = 0);
    ~Game();
    void Draw();
    // [NET] 한 틱의 입력(로컬/원격)을 적용합니다. Lockstep에선 수신된 입력을 이 경로로 합칩니다.
    void SubmitInput(uint8_t inputMask);
    // [NET] 틱을 1 증가시켜 중력/낙하 등 시간진행을 수행합니다(입력과 분리).
    void Tick();
    void MoveBlockDown();
    bool gameOver;
    int score;
    Music music;
    // GameState gameState;
    // Menu menu;

private:
    void MoveBlockLeft();
    void MoveBlockRight();
    void MoveBlockDrop();
    void DropExpectation();
    bool IsBlockOutside(const Block& block);
    void RotateBlock();
    void LockBlock();
    bool BlockFits(const Block& Block);
    void Reset();
    void UpdateScore(int linesCleared, int levelUp);
    std::vector<Block> blocks;
    std::vector<Block> GetAllBlocks();
    Grid grid;
    Block MakeGhostBlock(const Block& block);
    Block GetRandomBlock();
    XorShift64Star rng; // [NET] RNG 상태는 시뮬레이션 상태의 일부(스냅샷/해시에 포함 권장)
    // [NET] 현재 시뮬레이션 상태의 간단 해시(검증/로그)
    unsigned long long ComputeStateHash() const;
    Block currentBlock;
    Block ghostBlock;
    Block nextBlock;
    Sound rotateSound;
    Sound clearSound;
    Sound dropSound;
    Sound levelUpSound;
    // Tick-based gravity
    int gravityCounterTicks;      // [NET] 고정틱 기반 중력 카운터(결정론)
    int dropIntervalTicks;        // [NET] 자동 낙하 간격(틱)
};
