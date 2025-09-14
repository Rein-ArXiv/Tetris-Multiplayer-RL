#pragma once
#include <vector>
#include <map>
#include "position.h"
#include "colors.h"

class Block
{
public:
    Block();
    void Draw(int offsetX, int offsetY) const;
    void Move(int rows, int columns);
    std::vector<Position> GetCellPositions() const;
    void Rotate();
    void UndoRotation();
    int id;
    std::map<int, std::vector<Position>> cells;

private:
    int cellSize;
    int rotationState;
    std::vector<Color> colors;
    int rowOffset;
    int columnOffset;
public:
    // [NET] 결정론 상태 해시/스냅샷을 위해 내부 좌표/회전 접근자 제공
    int GetRotationState() const { return rotationState; }
    int GetRowOffset() const { return rowOffset; }
    int GetColumnOffset() const { return columnOffset; }
};
