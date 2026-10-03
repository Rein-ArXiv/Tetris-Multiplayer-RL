#pragma once

// [NET/RL] Pure, headless grid. No renderer, no rendering.
// Keep the original int[kRows][kCols] representation for state-hash parity.
// Arrays are contiguous regardless of whether their cells are int or uint8_t.
// Raw-byte hash compatibility also depends on int width and byte order; this
// in-memory layout is not a portable serialization format.
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
        // OOB 읽기를 방지한다. 정상 범위 입력의 셀 값이나 저장 레이아웃은 바꾸지 않는다.
        // 이 가드는 public grid 배열에 직접 접근하는 다른 호출부까지 보호하지 않는다.
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
