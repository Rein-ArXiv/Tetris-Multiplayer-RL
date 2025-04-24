#include "block.h"
#include <algorithm>

Block::Block(BlockType type) : type(type), rotationState(0), rowOffset(0), columnOffset(0) {
    InitializeBlocks();
}

void Block::InitializeBlocks() {
    switch (type) {
        case I_BLOCK:
            // I 블록
            cells[0] = {Position(1, 0), Position(1, 1), Position(1, 2), Position(1, 3)};
            cells[1] = {Position(0, 2), Position(1, 2), Position(2, 2), Position(3, 2)};
            cells[2] = {Position(2, 0), Position(2, 1), Position(2, 2), Position(2, 3)};
            cells[3] = {Position(0, 1), Position(1, 1), Position(2, 1), Position(3, 1)};
            break;
            
        case J_BLOCK:
            // J 블록
            cells[0] = {Position(0, 0), Position(1, 0), Position(1, 1), Position(1, 2)};
            cells[1] = {Position(0, 1), Position(0, 2), Position(1, 1), Position(2, 1)};
            cells[2] = {Position(1, 0), Position(1, 1), Position(1, 2), Position(2, 2)};
            cells[3] = {Position(0, 1), Position(1, 1), Position(2, 0), Position(2, 1)};
            break;
            
        case L_BLOCK:
            // L 블록
            cells[0] = {Position(0, 2), Position(1, 0), Position(1, 1), Position(1, 2)};
            cells[1] = {Position(0, 1), Position(1, 1), Position(2, 1), Position(2, 2)};
            cells[2] = {Position(1, 0), Position(1, 1), Position(1, 2), Position(2, 0)};
            cells[3] = {Position(0, 0), Position(0, 1), Position(1, 1), Position(2, 1)};
            break;
            
        case O_BLOCK:
            // O 블록 (회전해도 같은 모양)
            cells[0] = {Position(0, 0), Position(0, 1), Position(1, 0), Position(1, 1)};
            cells[1] = {Position(0, 0), Position(0, 1), Position(1, 0), Position(1, 1)};
            cells[2] = {Position(0, 0), Position(0, 1), Position(1, 0), Position(1, 1)};
            cells[3] = {Position(0, 0), Position(0, 1), Position(1, 0), Position(1, 1)};
            break;
            
        case S_BLOCK:
            // S 블록
            cells[0] = {Position(0, 1), Position(0, 2), Position(1, 0), Position(1, 1)};
            cells[1] = {Position(0, 1), Position(1, 1), Position(1, 2), Position(2, 2)};
            cells[2] = {Position(1, 1), Position(1, 2), Position(2, 0), Position(2, 1)};
            cells[3] = {Position(0, 0), Position(1, 0), Position(1, 1), Position(2, 1)};
            break;
            
        case T_BLOCK:
            // T 블록
            cells[0] = {Position(0, 1), Position(1, 0), Position(1, 1), Position(1, 2)};
            cells[1] = {Position(0, 1), Position(1, 1), Position(1, 2), Position(2, 1)};
            cells[2] = {Position(1, 0), Position(1, 1), Position(1, 2), Position(2, 1)};
            cells[3] = {Position(0, 1), Position(1, 0), Position(1, 1), Position(2, 1)};
            break;
            
        case Z_BLOCK:
            // Z 블록
            cells[0] = {Position(0, 0), Position(0, 1), Position(1, 1), Position(1, 2)};
            cells[1] = {Position(0, 2), Position(1, 1), Position(1, 2), Position(2, 1)};
            cells[2] = {Position(1, 0), Position(1, 1), Position(2, 1), Position(2, 2)};
            cells[3] = {Position(0, 1), Position(1, 0), Position(1, 1), Position(2, 0)};
            break;
            
        case GHOST_BLOCK:
            // 고스트 블록은 다른 블록을 복사하여 사용함
            break;
    }
}

void Block::Draw(WINDOW* win) const {
    std::vector<Position> positions = GetCellPositions();
    
    for (const auto& pos : positions) {
        // 블록 타입별로 다른 문자와 색상 사용
        chtype blockChar = ' ';
        int colorPair = static_cast<int>(type);
        
        switch (type) {
            case I_BLOCK:
                blockChar = 'I' | A_REVERSE;
                break;
            case J_BLOCK:
                blockChar = 'J' | A_REVERSE;
                break;
            case L_BLOCK:
                blockChar = 'L' | A_REVERSE;
                break;
            case O_BLOCK:
                blockChar = 'O' | A_REVERSE;
                break;
            case S_BLOCK:
                blockChar = 'S' | A_REVERSE;
                break;
            case T_BLOCK:
                blockChar = 'T' | A_REVERSE;
                break;
            case Z_BLOCK:
                blockChar = 'Z' | A_REVERSE;
                break;
            case GHOST_BLOCK:
                blockChar = '.' | A_REVERSE;
                break;
        }
        
        wattron(win, COLOR_PAIR(colorPair));
        mvwaddch(win, pos.row, pos.column, blockChar);
        wattroff(win, COLOR_PAIR(colorPair));
    }
}

void Block::Move(int rows, int columns) {
    rowOffset += rows;
    columnOffset += columns;
}

void Block::Rotate() {
    rotationState = (rotationState + 1) % 4;
}

void Block::UndoRotate() {
    rotationState = (rotationState + 3) % 4; // -1을 4로 나눈 나머지와 같음
}

std::vector<Position> Block::GetCellPositions() const {
    std::vector<Position> result;
    
    for (const auto& cell : cells[rotationState]) {
        Position pos = Position(cell.row + rowOffset, cell.column + columnOffset);
        result.push_back(pos);
    }
    
    return result;
}

Block Block::CreateGhostBlock(const Block& sourceBlock) {
    Block ghost(GHOST_BLOCK);
    ghost.cells = sourceBlock.cells;ㄴ
    ghost.rotationState = sourceBlock.rotationState;
    ghost.rowOffset = sourceBlock.rowOffset;
    ghost.columnOffset = sourceBlock.columnOffset;
    return ghost;
}

std::vector<Position> Block::GetCells() const {
    std::vector<Position> result;

    for (const auto& rel : cells[rotationState]){
        result.emplace_back(rel.row + rowOffset, rel.column + columnOffset);
    }

    return reuslt;
}

int Block::GetLeftmostColumn() const{
    auto cellsPos = GetCells();
    int minCol = INT_MAX;

    for (const auto& p : absCells){
        minCol = std::min(minCol, p.column);
    }
    return minCol;
}