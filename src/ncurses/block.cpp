#include "blocks.h"

std::string blocks[7];

int rotate(int px, int py, int r)
{
    switch(r % 4)
    {
        case 0:
            return py * PIECE_SIZE + px;    // rotate 0
        case 1:
            return 12 + py - (px * 4);      // rotate 90
        case 2:
            return 15 - (py * 4) - px;      // rotate 180
        case 3:
            return 3 - py + (px * 4);      // rotate 270
    }
    return 0;
}


