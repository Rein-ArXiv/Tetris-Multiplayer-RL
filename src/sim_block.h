#pragma once
#include <vector>
#include <map>
#include "position.h"

// [NET/RL] Pure, headless block state. No raylib, no audio, no rendering.
// Holds shape data (cells per rotation), position offsets, and rotation state.
// Used by SimGame for deterministic simulation (Colab training + Windows inference).
class SimBlock
{
public:
    SimBlock() : id(0), rotationState(0), rowOffset(0), columnOffset(0) {}

    void Move(int rows, int columns)
    {
        rowOffset += rows;
        columnOffset += columns;
    }

    std::vector<Position> GetCellPositions() const
    {
        const std::vector<Position>& tiles = cells.at(rotationState);
        std::vector<Position> movedTiles;
        movedTiles.reserve(tiles.size());
        for (const Position& item : tiles)
        {
            movedTiles.emplace_back(item.row + rowOffset, item.column + columnOffset);
        }
        return movedTiles;
    }

    void Rotate()
    {
        rotationState++;
        if (rotationState == static_cast<int>(cells.size()))
        {
            rotationState = 0;
        }
    }

    void UndoRotation()
    {
        rotationState--;
        if (rotationState == -1)
        {
            rotationState = static_cast<int>(cells.size()) - 1;
        }
    }

    // Public data — read by SimGame logic and by rendering wrappers.
    int id;
    std::map<int, std::vector<Position>> cells;
    int rotationState;
    int rowOffset;
    int columnOffset;

    // Accessors kept for state hash parity with old Block::GetRotationState etc.
    int GetRotationState() const { return rotationState; }
    int GetRowOffset() const { return rowOffset; }
    int GetColumnOffset() const { return columnOffset; }
};
