#include "text/raster_policy.h"
#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main(){
 std::size_t accepted=0,rejected=0;
 for(unsigned cp:{0u,65u,0xD7FFu,0xE000u,0x10FFFFu})
  for(int logical=1;logical<=2048;++logical)
   for(int eighth=1;eighth<=128;++eighth){
    const auto p=font_raster::plan(cp,logical,eighth/8.0);
    const int q=eighth<8?8:eighth;
    const int expected=(logical*q+4)/8;
    if(expected>2048){CHECK(!p);++rejected;continue;}
    CHECK(p&&p->logical_height==logical&&p->device_height==expected);
    CHECK((p->key>>32)==cp&&((p->key>>16)&65535u)==unsigned(logical)&&(p->key&65535u)==unsigned(expected));
    ++accepted;
   }
 CHECK(font_raster::plan(65,22,3.4)->device_height==74);
 CHECK(font_raster::plan(65,22,3.38)->key==font_raster::plan(65,22,3.4)->key);
 CHECK(font_raster::plan(65,16,1.0625)->device_height==18);
 CHECK(font_raster::plan(65,16,std::nextafter(1.0625,0.))->device_height==16);
 CHECK(font_raster::plan(65,1,1.125)->device_height==1); // quantized scale does not make row counts a multiple
 for(int n:{-1,0,2049,65552,(std::numeric_limits<int>::max)()})CHECK(!font_raster::plan(65,n,1));
 for(unsigned cp:{0xD800u,0xDFFFu,0x110000u,0xFFFFFFFFu})CHECK(!font_raster::plan(cp,16,1));
 for(double scale:{0.,-1.,2049.,(std::numeric_limits<double>::max)(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  std::feclearexcept(FE_ALL_EXCEPT);CHECK(!font_raster::plan(65,16,scale));CHECK(!(std::fetestexcept(FE_INVALID|FE_OVERFLOW)));
 }
 // Adjacent raw scales may share one device size; changing logical height must not alias.
 CHECK(font_raster::plan(65,16,2)->key!=font_raster::plan(65,32,1)->key);
 std::printf("Raster policy: %zu accepted, %zu rejected; integer oracle, packed-field identity, half-eighth boundary, invalid/huge inputs and FP exceptions passed\n",accepted,rejected);
}
