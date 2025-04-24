#ifndef POSITION_H
#define POSITION_H

class Position {
public:
    Position() : row(0), column(0) {}
    Position(int row, int column) : row(row), column(column) {}
    
    bool operator==(const Position& other) const {
        return row == other.row && column == other.column;
    }
    
    Position operator+(const Position& other) const {
        return Position(row + other.row, column + other.column);
    }
    
    int row;
    int column;
};

#endif // POSITION_H