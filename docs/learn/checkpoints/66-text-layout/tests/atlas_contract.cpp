#include "atlas_fake.h"
#include "renderer/glyph_atlas.h"
#include <algorithm>
static study_font::Glyph glyph(int w,int h,unsigned char value){study_font::Glyph g{};g.width=w;g.height=h;g.coverage.assign(std::size_t(w)*h,value);return g;}
int main(){
 int init_calls=0,insert_calls=0;
 // Keep the GL table alive: the atlas borrows it.
 auto gl=texture_api();
 {f={};const auto saved=f.state;{study_atlas::GlyphAtlas a(gl,16,16);CHECK(a.init());init_calls=f.call;CHECK(f.state==saved);const int before=f.call;auto r=a.insert(glyph(3,2,200));CHECK(r&&a.contains(*r)&&r->ink.x==1&&r->ink.y==1);insert_calls=f.call-before;CHECK(f.state==saved);
 const auto& pixels=f.textures.at(a.texture().name());for(int y=0;y<4;++y)for(int x=0;x<5;++x)CHECK(pixels.bytes[y*16+x]==((x>=1&&x<4&&y>=1&&y<3)?200:0));
 const auto previous=*r;const int calls=f.call;CHECK(a.insert(glyph(0,0,0)));CHECK(f.call==calls);
 CHECK(!a.insert(glyph(15,1,255))&&f.call==calls);CHECK(a.clear()&&!a.contains(previous));
 CHECK(std::all_of(pixels.bytes.begin(),pixels.bytes.end(),[](auto x){return x==0;}));
 auto again=a.insert(glyph(1,1,90));CHECK(again&&again->ink.x==1&&again->revision!=previous.revision);}
 CHECK(f.live.empty());}
 for(int fail=1;fail<=init_calls;++fail){f={};f.fail=fail;{study_atlas::GlyphAtlas a(gl,16,16);CHECK(!a.init()&&a.revision()==0);}CHECK(f.live.empty());}
 for(int fail=1;fail<=insert_calls;++fail){f={};{study_atlas::GlyphAtlas a(gl,16,16);CHECK(a.init());f.fail=f.call+fail;CHECK(!a.insert(glyph(3,2,200)));f.fail=0;auto r=a.insert(glyph(3,2,200));CHECK(r&&r->ink.x==1&&r->ink.y==1);}CHECK(f.live.empty());}
 f={};{study_atlas::GlyphAtlas a(gl,16,16),b(gl,16,16);CHECK(a.init()&&b.init());auto r=a.insert(glyph(2,2,255));CHECK(r&&!b.contains(*r));f.upload_error=true;CHECK(!a.clear()&&!a.contains(*r)&&!a.insert(glyph(1,1,10)));f.upload_error=false;CHECK(a.clear()&&a.insert(glyph(1,1,10)));}
 CHECK(f.live.empty());
 f={};{study_atlas::next_revision=(std::numeric_limits<std::uint64_t>::max)();study_atlas::GlyphAtlas a(gl,16,16),b(gl,16,16);CHECK(a.init());auto r=a.insert(glyph(1,1,255));CHECK(r);const auto calls=f.call;CHECK(!b.init()&&f.call==calls&&!a.clear()&&a.contains(*r));}
 CHECK(f.live.empty());std::printf("Atlas: init %d / insert %d failure stages; zero borders, state restore, rollback, foreign/stale regions and clear recovery passed\n",init_calls,insert_calls);
}
