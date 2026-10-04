#pragma once
namespace rotation_oracle {
// Row-major 4x4 expected masks, independent of the pivot formula.
inline constexpr unsigned masks[7][4]={
    {0x00f0,0x4444,0x0f00,0x2222},
    {0x0071,0x0226,0x0470,0x0322},
    {0x0074,0x0622,0x0170,0x0223},
    {0x0033,0x0033,0x0033,0x0033},
    {0x0036,0x0462,0x0360,0x0231},
    {0x0072,0x0262,0x0270,0x0232},
    {0x0063,0x0264,0x0630,0x0132}
};
inline bool fits(int k,int q,int row,int column,int obstacle) {
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(masks[k][q]&(1u<<(r*4+c))) {
        int y=row+r,x=column+c;
        if(y<0||y>=20||x<0||x>=10||y*10+x==obstacle)return false;
    }
    return true;
}
} // namespace rotation_oracle
