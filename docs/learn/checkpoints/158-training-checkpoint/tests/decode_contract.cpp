#include "renderer/image_decode.h"
#include "renderer/badge_pixels.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using namespace study_image;
static std::vector<std::uint8_t> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);CHECK(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){CHECK(argc==4);const auto fixtures=std::filesystem::u8path(argv[1]),scratch=std::filesystem::u8path(argv[3]);
 CHECK(!decode_memory(nullptr,1)&&decode_memory(nullptr,1).error==Error::invalid_input);
 std::uint8_t tiny=0;CHECK(decode_memory(&tiny,max_encoded_bytes+1).error==Error::encoded_limit);
 const unsigned char expected_rgb[]={255,0,0,255,0,255,0,255,0,0,255,255,11,22,33,255,44,55,66,255,77,88,99,255};
 auto rgb=decode_file(fixtures/"rgb.png");CHECK(rgb&&rgb.error==Error::none&&rgb.image->width==3&&rgb.image->height==2&&rgb.image->source_channels==3);
 CHECK(rgb.image->pixels.size()==24&&std::memcmp(rgb.image->pixels.data(),expected_rgb,24)==0);
 const unsigned char expected_rgba[]={255,0,0,255,0,255,0,128,0,0,255,0,11,22,33,44};
 auto rgba=decode_file(fixtures/"rgba.png");CHECK(rgba&&rgba.image->source_channels==4&&std::memcmp(rgba.image->pixels.data(),expected_rgba,16)==0);
 auto gray=decode_file(fixtures/"gray.png");CHECK(gray&&gray.image->source_channels==1&&gray.image->pixels.size()==24);
 const unsigned char values[]={0,64,128,192,224,255};for(unsigned i=0;i<6;++i){for(unsigned c=0;c<3;++c)CHECK(gray.image->pixels[i*4+c]==values[i]);CHECK(gray.image->pixels[i*4+3]==255);}
 auto jpeg=decode_file(fixtures/"color.jpg");CHECK(jpeg&&jpeg.image->width==8&&jpeg.image->height==8&&jpeg.image->source_channels==3);
 for(unsigned i=0;i<64;++i){CHECK(std::abs(int(jpeg.image->pixels[i*4])-64)<=2&&std::abs(int(jpeg.image->pixels[i*4+1])-128)<=2&&std::abs(int(jpeg.image->pixels[i*4+2])-192)<=2&&jpeg.image->pixels[i*4+3]==255);}
 auto badge=decode_file(std::filesystem::u8path(argv[2]));CHECK(badge&&badge.image->pixels.size()==256&&std::equal(badge.image->pixels.begin(),badge.image->pixels.end(),study_texture::badge_pixels.begin()));
 auto encoded=read(fixtures/"rgb.png");const auto saved=encoded;auto memory=decode_memory(encoded.data(),encoded.size());CHECK(memory);encoded.assign(encoded.size(),0);CHECK(std::memcmp(memory.image->pixels.data(),expected_rgb,24)==0);
 CHECK(decode_file(fixtures/"too-large.png").error==Error::dimensions);
 CHECK(decode_file(fixtures/"corrupt.png").error==Error::decode_failed);
 CHECK(decode_file(fixtures/"not-image.dat").error==Error::decode_failed);
 CHECK(decode_file(fixtures/"not-present.png").error==Error::file_io);
 // Decoder acceptance is not strict PNG conformance: examine every prefix,
 // requiring a consistent result and complete pixels for any accepted prefix.
 for(std::size_t n=0;n<saved.size();++n){auto a=decode_memory(saved.data(),n);if(a)CHECK(a.error==Error::none&&a.image->pixels.size()==24);else CHECK(a.error!=Error::none);}
 std::filesystem::create_directories(scratch);
 auto named=scratch/std::filesystem::u8path(u8"교재 아이콘.txt");{std::ofstream out(named,std::ios::binary);out.write(reinterpret_cast<const char*>(saved.data()),static_cast<std::streamsize>(saved.size()));CHECK(out);}
 CHECK(decode_file(named));
 auto huge=scratch/"oversized.bin";{std::ofstream out(huge,std::ios::binary);out.seekp(max_encoded_bytes);out.put('x');CHECK(out);}
 CHECK(decode_file(huge).error==Error::encoded_limit);
 auto empty=scratch/"empty.png";{std::ofstream out(empty);CHECK(out);}CHECK(decode_file(empty).error==Error::invalid_input);
 std::filesystem::remove(named);std::filesystem::remove(huge);std::filesystem::remove(empty);
 std::puts("PNG RGB/RGBA/gray, JPEG, channels/alpha/orientation, owned output, corrupt/limited input and file paths passed");
}
