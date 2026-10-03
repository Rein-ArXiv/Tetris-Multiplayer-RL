#include "net/seed_protocol.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace study_net;
int main() {
 const auto hello=make_hello();CHECK(hello.type==10 && hello.size==6 && valid_hello(hello));
 const uint8_t expected_hello[]{1,0,1,0,0,0};for(size_t i=0;i<6;++i)CHECK(hello.payload[i]==expected_hello[i]);
 for(size_t n=0;n<=33;++n){auto bad=hello;bad.size=n;CHECK(valid_hello(bad)==(n==6));}
 for(size_t i=0;i<6;++i){auto bad=hello;bad.payload[i]^=1;CHECK(!valid_hello(bad));}
 Config config{0x0102030405060708ull,120,2};Frame f;
 CHECK(encode_config(kTypeOffer,config,f));
 const uint8_t expected[]{1,0,1,0,0,0,8,7,6,5,4,3,2,1,120,0,0,0,2,2};
 CHECK(f.size==20);for(size_t i=0;i<20;++i)CHECK(f.payload[i]==expected[i]);
 Config decoded{9,10,11};CHECK(decode_config(f,kTypeOffer,decoded) && equal_config(decoded,config));
 const Config sentinel{9,10,11};
 for(size_t n=0;n<=33;++n){auto bad=f;bad.size=n;decoded=sentinel;bool ok=decode_config(bad,kTypeOffer,decoded);CHECK(ok==(n==20));if(!ok)CHECK(equal_config(decoded,sentinel));}
 for(size_t pos: {size_t(0),size_t(2),size_t(19)}) {auto bad=f;bad.payload[pos]=0;decoded=sentinel;CHECK(!decode_config(bad,kTypeOffer,decoded)&&equal_config(decoded,sentinel));}
 for(unsigned role=0;role<256;++role){auto bad=f;bad.payload[19]=uint8_t(role);decoded=sentinel;CHECK(decode_config(bad,kTypeOffer,decoded)==(role==2));}
 for(uint64_t seed: {uint64_t(0),uint64_t(1),(std::numeric_limits<uint64_t>::max)()})
  for(uint32_t ticks: {0u,120u,600u})for(uint8_t delay: {uint8_t(0),uint8_t(2),uint8_t(30)}) {
   Config c{seed,ticks,delay};CHECK(encode_config(kTypeAck,c,f));CHECK(decode_config(f,kTypeAck,decoded)&&equal_config(c,decoded));
  }
 Frame preserved=f;CHECK(!encode_config(kTypeOffer,{1,601,2},f) && f.payload==preserved.payload && f.type==preserved.type);
 CHECK(!encode_config(kTypeOffer,{1,120,31},f));CHECK(!encode_config(kTypeHello,config,f));
 CHECK(!decode_config(f,kTypeOffer,decoded));CHECK(!decode_config(f,kTypeHello,decoded));
 CHECK(encode_config(kTypeOffer,config,f));f.payload[14]=0x59;f.payload[15]=2;decoded=sentinel;
 CHECK(!decode_config(f,kTypeOffer,decoded)&&equal_config(decoded,sentinel)); // countdown 601
 CHECK(encode_config(kTypeOffer,config,f));f.payload[18]=31;CHECK(!decode_config(f,kTypeOffer,decoded));
 std::puts("Seed codec: fixed LE vectors, full receiver-role space, exact sizes, bounds, zero/max seed and output preservation passed");
}
