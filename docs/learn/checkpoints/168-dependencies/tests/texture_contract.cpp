#include "renderer/texture.h"
#include "renderer/badge_pixels.h"
#include "texture_fake.h"
#include <climits>
#include <limits>
#include <type_traits>
using namespace study_texture;
static_assert(!std::is_copy_constructible_v<Texture> && !std::is_move_constructible_v<Texture>);
int main(){
 CHECK(rgba_byte_count(3,2)==24&&!rgba_byte_count(0,2)&&!rgba_byte_count(-1,2));
 const auto max=rgba_byte_count(INT_MAX,INT_MAX);
 if(std::numeric_limits<std::size_t>::digits>=64)CHECK(max&&*max==std::uint64_t(INT_MAX)*INT_MAX*4);
 else CHECK(!max);
 unsigned char data[24];for(unsigned i=0;i<24;++i)data[i]=static_cast<unsigned char>(i);
 CHECK(valid({data,24,3,2})&&!valid({data,23,3,2})&&!valid({data,25,3,2})&&!valid({nullptr,24,3,2}));
 CHECK(badge_pixels.size()==256&&badge_pixels[0]==230&&badge_pixels[4]==230&&badge_pixels[32]==20);
 for(unsigned i=3;i<256;i+=4)CHECK(badge_pixels[i]==255);
 auto gl=texture_api();const auto initial=f.state;int full_calls=0;
 {Texture t(gl);CHECK(!t.upload({data,23,3,2})&&f.call==0);CHECK(t.upload({data,24,3,2}));
  CHECK(t.name()==1&&t.width()==3&&t.height()==2&&f.state==initial&&f.copy.size()==24);
  CHECK(std::memcmp(f.copy.data(),data,24)==0);full_calls=f.call;CHECK(!t.upload({data,24,3,2})&&f.call==full_calls);
  data[0]=255;CHECK(f.copy[0]==0);
 }CHECK(f.live.empty()&&f.deletes==1);
 for(int fail=1;fail<=full_calls;++fail){f={};f.fail=fail;
  {Texture t(gl);CHECK(!t.upload({data,24,3,2})&&t.name()==0&&t.width()==0&&t.height()==0);}
  CHECK(f.live.empty());
  // If only an operation before restoration fails, all saved states return.
  if(fail<=full_calls-6)CHECK(f.state==initial);
 }
 f={};f.zero=true;{Texture t(gl);CHECK(!t.upload({data,24,3,2})&&f.deletes==0&&f.uploads==0);}
 f={};f.error=0x0502;{Texture t(gl);CHECK(!t.upload({data,24,3,2})&&f.call==0);}
 f={};f.state[MaxTextureSize]=2;{Texture t(gl);CHECK(!t.upload({data,24,3,2})&&f.gens==0);}
 f={};f.upload_error=true;{Texture t(gl);CHECK(!t.upload({data,24,3,2})&&f.live.empty()&&f.state==initial);f.upload_error=false;CHECK(t.upload({data,24,3,2}));}CHECK(f.live.empty());
 std::printf("RGBA view, copied bytes, limits, all %d GL operation failures, restoration and ownership passed\n",full_calls);
}
