#include "renderer/badge_pixels.h"
#include <cstdio>
int main(){
 using namespace study_texture;
 const RgbaView pixels{badge_pixels.data(),badge_pixels.size(),badge_width,badge_height};
 if(!valid(pixels))return 1;
 std::printf("%dx%d RGBA8: %zu bytes; top-left=(%u,%u,%u,%u)\n",pixels.width,pixels.height,pixels.bytes,unsigned(pixels.data[0]),unsigned(pixels.data[1]),unsigned(pixels.data[2]),unsigned(pixels.data[3]));
}
