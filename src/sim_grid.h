#pragma once

// [NET/RL] Pure, headless grid. No raylib, no rendering.
// Layout (int grid[kRows][kCols]) must match the old Grid class so that
// ComputeStateHash produces identical bytes when fnv1a64 is applied to the
// contiguous memory range.
class SimGrid
{
public:
    static constexpr int kRows = 20;
    static constexpr int kCols = 10;

    SimGrid() { Initialize(); }

    void Initialize()
    {
        for (int row = 0; row < kRows; row++)
        {
            for (int column = 0; column < kCols; column++)
            {
                grid[row][column] = 0;
            }
        }
    }

    bool IsCellOutside(int row, int column) const
    {
        if (row >= 0 && row < kRows && column >= 0 && column < kCols)
        {
            return false;
        }
        return true;
    }

    bool IsCellEmpty(int row, int column) const
    {
        // 방어적 경계 검사: 범위 밖 좌표는 '비어있지 않음'(막힘)으로 처리한다.
        // 호출부는 보통 IsCellOutside 로 선검사하지만, 만약 무경계 접근이 들어와도
        // OOB 읽기를 방지한다. 해시 대상은 grid 내용뿐이므로 결정성/리플레이 호환성에
        // 영향이 없다.
        if (IsCellOutside(row, column))
        {
            return false;
        }
        if (grid[row][column] == 0 || grid[row][column] == 8)
        {
            return true;
        }
        return false;
    }

    int ClearFullRows()
    {
        int completed = 0;
        for (int row = kRows - 1; row >= 0; row--)
        {
            if (IsRowFull(row))
            {
                ClearRow(row);
                completed++;
            }
            else if (completed > 0)
            {
                MoveRowDown(row, completed);
            }
        }
        return completed;
    }

    // Public: matches old Grid::grid layout for hash parity.
    int grid[kRows][kCols];

private:
    bool IsRowFull(int row) const
    {
        for (int column = 0; column < kCols; column++)
        {
            if (grid[row][column] == 0)
            {
                return false;
            }
        }
        return true;
    }

    void ClearRow(int row)
    {
        for (int column = 0; column < kCols; column++)
        {
            grid[row][column] = 0;
        }
    }

    void MoveRowDown(int row, int numRowsDown)
    {
        for (int column = 0; column < kCols; column++)
        {
            grid[row + numRowsDown][column] = grid[row][column];
            grid[row][column] = 0;
        }
    }
};
