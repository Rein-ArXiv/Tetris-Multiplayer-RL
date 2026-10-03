#include "renderer/image_decode.h"
#include "stbi_alloc_hooks.h"
#include <fstream>
#include <iterator>
#include <new>
#include <cstdio>
#include <cstdlib>
static bool reject_copy=false;
void* operator new(std::size_t size){if(reject_copy&&size==24){reject_copy=false;throw std::bad_alloc();}if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
int main(int argc,char** argv){CHECK(argc==2);std::ifstream file(argv[1],std::ios::binary);CHECK(file);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
 reject_copy=true;auto result=study_image::decode_memory(bytes.data(),bytes.size());
 CHECK(!reject_copy&&!result&&result.error==study_image::Error::allocation&&stbi_blocks==0);
 auto good=study_image::decode_memory(bytes.data(),bytes.size());CHECK(good&&good.image->pixels.size()==24&&stbi_blocks==0);
 std::puts("Real stb allocation tracking: vector copy bad_alloc releases decoded pixels; retry succeeds");
}
