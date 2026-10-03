#include "renderer/image_decode.h"
#include <cstdio>
int main(int argc,char** argv){
    if(argc!=2){std::fprintf(stderr,"usage: decode_demo IMAGE\n");return 2;}
    const auto result=study_image::decode_file(std::filesystem::u8path(argv[1]));
    if(!result){std::fprintf(stderr,"decode: %s\n",study_image::error_text(result.error));return 1;}
    const auto& image=*result.image;
    std::printf("%dx%d source channels=%d -> RGBA8 %zu bytes; first=(%u,%u,%u,%u)\n",
        image.width,image.height,image.source_channels,image.pixels.size(),
        unsigned(image.pixels[0]),unsigned(image.pixels[1]),unsigned(image.pixels[2]),unsigned(image.pixels[3]));
}
