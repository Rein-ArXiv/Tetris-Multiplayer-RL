#ifndef GRID_H
#define GRID_H

#include <vector>
#include <ncurses.h>
#include "position.h"

class Grid {
public:
    Grid(int height = 20, int width = 10);
    
    // 그리드 초기화 및 표시
    void Initialize();
    void Draw(WINDOW* win) const;
    
    // 셀 관련 함수
    bool IsCellOutside(int row, int column) const;
    bool IsCellEmpty(int row, int column) const;
    
    // 라인 제거 관련 함수
    int ClearFullRows();
    bool IsRowFull(int row) const;
    void ClearRow(int row);
    void MoveRowDown(int row, int numRows);
    
    // 그리드 통계 함수
    int GetHeight() const { return height; }
    int GetWidth() const { return width; }
    int GetAverageHeight() const;
    int GetHoleCount() const;
    
    // 전체 그리드 상태 관리
    std::vector<std::vector<int>> cells;
    
    void AddGarbageLines(int count);
    
private:
    int height;
    int width;
};

#endif // GRID_H