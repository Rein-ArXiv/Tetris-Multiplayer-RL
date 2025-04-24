#ifndef BLOCK_H
#define BLOCK_H

#include <vector>
#include <array>
#include <ncurses.h>
#include "position.h"

// 블록 타입 정의
enum BlockType {
    I_BLOCK = 1,
    J_BLOCK,
    L_BLOCK,
    O_BLOCK,
    S_BLOCK,
    T_BLOCK,
    Z_BLOCK,
    GHOST_BLOCK
};

class Block {
public:
    Block(BlockType type = I_BLOCK);
    
    // 기본 동작
    void Draw(WINDOW* win) const;
    void Move(int rows, int columns);
    void Rotate();
    void UndoRotate();
    std::vector<Position> GetCellPositions() const;
    
    // Block 속성
    BlockType type;
    int rotationState;
    std::array<std::vector<Position>, 4> cells; // 4개의 회전 상태
    int rowOffset;
    int columnOffset;
    
    // Ghost Block (Drop 위치 보여주는 Block)
    static Block CreateGhostBlock(const Block& sourceBlock);
    
private:
    void InitializeBlocks();
};

#endif // BLOCK_H