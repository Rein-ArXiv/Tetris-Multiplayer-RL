#pragma once
#include <array>
// Independent row-major bit masks and plain integer board, no teaching helpers.
namespace locking_oracle {
inline constexpr unsigned masks[]={0xF0,0x71,0x74,0x33,0x36,0x72,0x63};
using Board=std::array<bool,200>;
inline bool fits(const Board& b,int kind,int row,int column) {
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(masks[kind]&(1u<<(r*4+c))) {
        const int y=row+r,x=column+c;
        if(y<0||y>=20||x<0||x>=10||b[y*10+x])return false;
    }
    return true;
}
inline void fill(Board& b,int kind,int row,int column) {
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(masks[kind]&(1u<<(r*4+c)))b[(row+r)*10+column+c]=true;
}
struct Pile {
    Board board{};
    int kind=0;
    int column=3;
    int locks=0;
    bool finished=false;
    explicit Pile(int k):kind(k),column(k==3?4:3) { board[0]=board[12]=board[104]=board[199]=true; }
    void next_lock() {
        if(finished)return;
        int row=0;
        while(fits(board,kind,row+1,column))++row;
        fill(board,kind,row,column);++locks;
        finished=!fits(board,kind,0,column);
    }
};
} // namespace locking_oracle
