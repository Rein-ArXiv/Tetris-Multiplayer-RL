#include "grid.h"
#include <algorithm>

Grid::Grid(int height, int width) : height(height), width(width) {
    // 그리드 크기에 맞게 셀 벡터 초기화
    cells.resize(height, std::vector<int>(width, 0));
    Initialize();
}

void Grid::Initialize() {
    // 모든 셀을 0으로 초기화 (빈 셀)
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            cells[row][col] = 0;
        }
    }
}

void Grid::Draw(WINDOW* win) const {
    // 그리드의 모든 셀을 화면에 표시
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int cellValue = cells[row][col];
            
            // 셀 값에 따라 다른 표시
            if (cellValue == 0) {
                // 빈 셀
                mvwaddch(win, row, col, ' ');
            } else {
                // 블록이 있는 셀
                chtype blockChar = ' ';
                int colorPair = cellValue;
                
                switch (cellValue) {
                    case 1: blockChar = 'I' | A_REVERSE; break;
                    case 2: blockChar = 'J' | A_REVERSE; break;
                    case 3: blockChar = 'L' | A_REVERSE; break;
                    case 4: blockChar = 'O' | A_REVERSE; break;
                    case 5: blockChar = 'S' | A_REVERSE; break;
                    case 6: blockChar = 'T' | A_REVERSE; break;
                    case 7: blockChar = 'Z' | A_REVERSE; break;
                    case 9: blockChar = '#'; break; // 쓰레기 라인
                    default: blockChar = '*' | A_REVERSE; break;
                }
                
                wattron(win, COLOR_PAIR(colorPair));
                mvwaddch(win, row, col, blockChar);
                wattroff(win, COLOR_PAIR(colorPair));
            }
        }
    }
}

bool Grid::IsCellOutside(int row, int column) const {
    // 셀이 그리드 범위를 벗어나는지 확인
    return row < 0 || row >= height || column < 0 || column >= width;
}

bool Grid::IsCellEmpty(int row, int column) const {
    // 셀이 범위를 벗어나는 경우 비어있지 않음
    if (IsCellOutside(row, column)) {
        return false;
    }
    
    // 셀의 값이 0인 경우 비어있음
    return cells[row][column] == 0;
}

int Grid::ClearFullRows() {
    int rowsCleared = 0;
    
    // 아래에서 위로 행을 확인하며 꽉 찬 행을 제거
    for (int row = height - 1; row >= 0; row--) {
        if (IsRowFull(row)) {
            ClearRow(row);
            rowsCleared++;
        } else if (rowsCleared > 0) {
            // 이미 지워진 행이 있다면, 현재 행을 아래로 이동
            MoveRowDown(row, rowsCleared);
        }
    }
    
    return rowsCleared;
}

bool Grid::IsRowFull(int row) const {
    // 행의 모든 셀이 채워져 있는지 확인
    if (row < 0 || row >= height) {
        return false;
    }
    
    for (int col = 0; col < width; col++) {
        if (cells[row][col] == 0) {
            return false;
        }
    }
    
    return true;
}

void Grid::ClearRow(int row) {
    // 행의 모든 셀을 0으로 설정하여 지움
    if (row >= 0 && row < height) {
        for (int col = 0; col < width; col++) {
            cells[row][col] = 0;
        }
    }
}

void Grid::MoveRowDown(int row, int numRows) {
    // 행을 지정된 수만큼 아래로 이동
    if (row >= 0 && row < height && row + numRows < height) {
        for (int col = 0; col < width; col++) {
            cells[row + numRows][col] = cells[row][col];
            cells[row][col] = 0;
        }
    }
}

int Grid::GetAverageHeight() const {
    int totalHeight = 0;
    int count = 0;
    
    // 각 열의 최고 높이를 측정하여 평균 계산
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            if (cells[row][col] != 0) {
                totalHeight += (height - row);
                count++;
                break;
            }
        }
    }
    
    return count > 0 ? totalHeight / count : 0;
}

int Grid::GetHoleCount() const {
    int holes = 0;
    
    // 각 열에서 블록 아래의 빈 공간 수 계산
    for (int col = 0; col < width; col++) {
        bool foundBlock = false;
        
        for (int row = 0; row < height; row++) {
            if (cells[row][col] != 0) {
                foundBlock = true;
            } else if (foundBlock) {
                // 이미 블록을 발견했는데 빈 공간이 있으면 구멍으로 카운트
                holes++;
            }
        }
    }
    
    return holes;
}

void Grid::AddGarbageLines(int count) {
    if (count <= 0) return;
    
    // 상단의 'count'만큼의 행을 제거
    for (int row = 0; row < count; row++) {
        for (int col = 0; col < width; col++) {
            if (cells[row][col] != 0) {
                // 게임 오버 상태가 될 수 있음
                return;
            }
        }
    }
    
    // 기존 행을 위로 이동
    for (int row = count; row < height; row++) {
        for (int col = 0; col < width; col++) {
            cells[row - count][col] = cells[row][col];
        }
    }
    
    // 하단에 쓰레기 라인 추가
    for (int row = height - count; row < height; row++) {
        for (int col = 0; col < width; col++) {
            // 랜덤하게 하나의 빈 공간 생성 (플레이어가 게임을 계속할 수 있도록)
            int holeCol = rand() % width;
            if (col == holeCol) {
                cells[row][col] = 0;
            } else {
                cells[row][col] = 9; // 쓰레기 라인 표시
            }
        }
    }
}