#pragma once
#include "grid.h"
#include "blocks.cpp"

class Game{
public:
    Game();
    ~Game();
    void Draw();
    void HandleInput();
    void MoveBlockDown();
    bool gameOver;
    int score;
    Music music;

private:
    void MoveBlockLeft();
    void MoveBlockRight();
    void MoveBlockDrop();
    bool IsBlockOutside();
    void RotateBlock();
    void LockBlock();
    bool BlockFits();
    void Reset();
    void UpdateScore(int linesCleared, int levelUp);
    std::vector<Block> blocks;
    std::vector<Block> GetAllBlocks();
    Grid grid;
    Block GetRandomBlock();
    Block currentBlock;
    Block nextBlock;
    Sound rotateSound;
    Sound clearSound;
    Sound dropSound;
    Sound levelUpSound;
};