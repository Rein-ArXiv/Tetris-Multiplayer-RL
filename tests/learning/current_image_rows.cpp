#include "renderer/image_rows.h"
#include <array>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
int main(){using namespace image_detail;
 CHECK(rgba_storage_bytes(3,2)==24&&!rgba_storage_bytes(0,1)&&!rgba_storage_bytes(-1,1));
 CHECK(!rgba_storage_bytes(INT_MAX,INT_MAX));
 for(int w=1;w<=19;++w)for(int h=1;h<=13;++h)for(int pad=0;pad<8;++pad)for(int sign:{-1,1}){
  const auto row=w*4,step=row+pad;std::vector<unsigned char> source(std::size_t(step)*h,0xEE),out(std::size_t(row)*h,0xA5),expected(out.size());
  auto* scan0=source.data()+(sign<0?std::size_t(step)*(h-1):0);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto* q=scan0+y*sign*step+x*4;q[0]=static_cast<unsigned char>(x);q[1]=static_cast<unsigned char>(y);q[2]=static_cast<unsigned char>(x+y);q[3]=static_cast<unsigned char>(200+x);
   auto k=std::size_t(y*w+x)*4;expected[k]=q[2];expected[k+1]=q[1];expected[k+2]=q[0];expected[k+3]=q[3];}
  CHECK(copy_bgra_rows(scan0,sign*step,w,h,out.data(),out.size())&&out==expected);
 }
 std::array<unsigned char,8> source{},out;out.fill(99);const auto saved=out;
 for(auto stride:{std::ptrdiff_t{0},std::ptrdiff_t{3},(std::numeric_limits<std::ptrdiff_t>::min)(),(std::numeric_limits<std::ptrdiff_t>::max)()}){
  CHECK(!copy_bgra_rows(source.data(),stride,1,2,out.data(),out.size())&&out==saved);
 }
 CHECK(!copy_bgra_rows(nullptr,4,1,2,out.data(),8)&&out==saved);
 CHECK(!copy_bgra_rows(source.data(),4,1,2,out.data(),7)&&out==saved);
 std::puts("BGRA row conversion: 3952 positive/negative padded layouts, alpha and overflow failure preservation passed");
}
