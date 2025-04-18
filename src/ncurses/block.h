#ifndef BLOCK_H
#define BLOCK_H

#include <string>

const int PIECE_SIZE = 4;

extern std::string blocks[7];

int rotate(int px, int py, int r);

void initialize_blocks()

#endif  // BLOCK_H
