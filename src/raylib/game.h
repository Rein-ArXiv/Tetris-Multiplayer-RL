#pragma once
#include "grid.h"
#include "blocks.cpp"
#include "menu.h"

/*

enum GameState{
    MENU,
    PLAYING,
    GAME_OVER
};

*/


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
    Block currentBlock;
    Block ghostBlock;
    Block nextBlock;
    Sound rotateSound;
    Sound clearSound;
    Sound dropSound;
    Sound levelUpSound;
};