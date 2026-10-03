#include "text/utf8.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
// RFC byte-range table oracle: constrain the second byte before assembling.
static study_utf8::Result oracle(std::string_view s){
 using study_utf8::Status;
 if(s.empty())return {};
 const auto b=[&](std::size_t i){return static_cast<unsigned char>(s[i]);};
 const study_utf8::Result bad{0xFFFD,1,Status::invalid};
 if(b(0)<128)return {b(0),1,Status::scalar};
 std::size_t n=0;unsigned low=128,high=191,cp=0;
 if(b(0)>=194&&b(0)<=223){n=2;cp=b(0)-192;}
 else if(b(0)>=224&&b(0)<=239){n=3;cp=b(0)-224;if(b(0)==224)low=160;if(b(0)==237)high=159;}
 else if(b(0)>=240&&b(0)<=244){n=4;cp=b(0)-240;if(b(0)==240)low=144;if(b(0)==244)high=143;}
 else return bad;
 if(s.size()<n||b(1)<low||b(1)>high)return bad;
 for(std::size_t i=1;i<n;++i){if(b(i)<128||b(i)>191)return bad;cp=cp*64+(b(i)-128);}
 return {char32_t(cp),n,Status::scalar};
}
static void compare(std::string_view s){
 const auto actual=study_utf8::decode_first(s),expected=oracle(s);
 CHECK(actual.status==expected.status&&actual.bytes==expected.bytes&&actual.codepoint==expected.codepoint);
 CHECK(s.empty()?actual.bytes==0:actual.bytes>=1&&actual.bytes<=4&&actual.bytes<=s.size());
}
// Arithmetic encoder independent of decoder's bit masks.
static std::string encode(unsigned cp){
 if(cp<128)return std::string(1,char(cp));
 if(cp<2048)return {char(192+cp/64),char(128+cp%64)};
 if(cp<65536)return {char(224+cp/4096),char(128+cp/64%64),char(128+cp%64)};
 return {char(240+cp/262144),char(128+cp/4096%64),char(128+cp/64%64),char(128+cp%64)};
}
int main(){using study_utf8::decode_first;using study_utf8::Status;
 CHECK(decode_first({}).status==Status::end);
 std::size_t valid=0;
 for(unsigned cp=0;cp<=0x10FFFF;++cp){
  if(cp>=0xD800&&cp<=0xDFFF)continue;
  const auto bytes=encode(cp);const auto d=decode_first(bytes);
  CHECK(d.status==Status::scalar&&d.codepoint==cp&&d.bytes==bytes.size());
  for(std::size_t n=1;n<bytes.size();++n){auto part=decode_first(std::string_view(bytes).substr(0,n));CHECK(part.status==Status::invalid&&part.bytes==1);}
  ++valid;
 }
 char data[8]{};std::size_t pairs=0;
 for(int a=0;a<256;++a){data[0]=char(a);compare({data,1});for(int b=0;b<256;++b){data[1]=char(b);compare({data,2});++pairs;}}
 std::uint32_t rng=1;
 for(unsigned run=0;run<200000;++run){
  for(auto& b:data){rng=rng*1664525u+1013904223u;b=char(rng>>24);}
  std::string_view view(data,1+run%8);
  while(!view.empty()){compare(view);view.remove_prefix(decode_first(view).bytes);}
 }
 for(const auto& s:std::vector<std::string>{{char(0xC0),char(0x80)},{char(0xE0),char(0x80),char(0xAF)},
     {char(0xED),char(0xA0),char(0x80)},{char(0xF4),char(0x90),char(0x80),char(0x80)}}){
  CHECK(decode_first(s).status==Status::invalid&&decode_first(s).bytes==1);
 }
 const char recovery[]={char(0xE2),char(0x82),'A'};std::string_view rest(recovery,3);
 for(auto cp:{char32_t(0xFFFD),char32_t(0xFFFD),U'A'}){auto d=decode_first(rest);CHECK(d.codepoint==cp&&d.bytes==1);rest.remove_prefix(d.bytes);}
 const char nul[]={'A',0,'B'};CHECK(decode_first({nul+1,2}).status==Status::scalar&&decode_first({nul+1,2}).codepoint==0);
 CHECK(decode_first(u8"\uFFFD").status==Status::scalar&&decode_first(u8"\uFFFD").bytes==3);
 CHECK(decode_first(u8"\uFFFF").status==Status::scalar); // Noncharacter is still a scalar.
 CHECK(decode_first(u8"\uFEFF").codepoint==0xFEFF); // BOM removal is a separate policy.
 std::printf("UTF-8: %zu scalar roundtrips/all truncations, 256 singles/%zu pairs, 200000 streams, recovery/NUL/U+FFFD passed\n",valid,pairs);
}
