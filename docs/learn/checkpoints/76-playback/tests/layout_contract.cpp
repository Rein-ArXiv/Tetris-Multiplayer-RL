#include "text/paragraph.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
static bool close(float a,float b){return std::abs(a-b)<.0002f;}
static bool same(const study_layout::Result& a,const study_layout::Result& b){
 if(a.count!=b.count||a.lines!=b.lines||a.width!=b.width||a.height!=b.height||bool(a.ink)!=bool(b.ink))return false;
 for(std::size_t i=0;i<16;++i)if(a.items[i].pen_x!=b.items[i].pen_x||a.items[i].baseline!=b.items[i].baseline)return false;
 return a.line_widths==b.line_widths&&(!a.ink||(a.ink->x==b.ink->x&&a.ink->y==b.ink->y&&a.ink->w==b.ink->w&&a.ink->h==b.ink->h));
}
int main(int argc,char** argv){
 using namespace study_layout;
 Layout sample({8,-2,3});CHECK(sample.valid());CHECK(sample.add(10,0,{-1,-8,9,10}));CHECK(sample.add(8,-2,{0,-7,9,8}));
 auto r=sample.result();CHECK(r.items[1].pen_x==8&&r.width==16&&r.ink&&r.ink->x==-1&&r.ink->w==18&&r.height==10);
 CHECK(sample.newline()&&!sample.has_previous());CHECK(sample.add(5,0,{0,0,0,0}));r=sample.result();CHECK(r.lines==2&&r.width==16&&r.height==23&&r.items[2].baseline==13&&r.line_widths[1]==5);
 CHECK(sample.newline());CHECK(sample.result().height==36&&sample.result().lines==3);
 Layout shrink({8,-2,0});CHECK(shrink.add(10,0,{0,0,10,1})&&shrink.add(0,-5,{}));CHECK(shrink.result().width==5&&shrink.result().ink->w==10);
 int cases=0;
 // Separate scalar arithmetic, both a continued line and a newline reset.
 for(int a=0;a<=8;++a)for(int b=0;b<=8;++b)for(int k=-8;k<=8;++k)for(bool split:{false,true}){
  Layout l({8,-2,3});CHECK(l.add(float(a),0,{-1,-3,2,4}));if(split)CHECK(l.newline());
  const auto before=l.result();const float kern=split?0.f:float(k);const float origin=(split?0.f:float(a))+kern;
  const bool expected=origin+b>=0;CHECK(l.add(float(b),kern,{2,-1,3,2})==expected);
  if(!expected){CHECK(same(before,l.result()));continue;}
  const auto out=l.result();CHECK(out.items[1].pen_x==origin&&out.items[1].baseline==(split?13:0));
  CHECK(out.width==(split?float(std::max(a,b)):origin+b));
  const float left=std::min(-1.f,origin+2),right=std::max(1.f,origin+5);
  CHECK(out.ink&&out.ink->x==left&&out.ink->w==right-left);++cases;
 }
 Layout limits({8,-2,3});const auto initial=limits.result();CHECK(!limits.add(1,1,{})&&same(initial,limits.result()));
 for(float bad:{-1.f,4097.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})CHECK(!limits.add(bad,0,{})&&same(initial,limits.result()));
 CHECK(!limits.add(1,0,{0,0,-1,1})&&same(initial,limits.result()));
 for(int i=0;i<16;++i){CHECK(limits.add(1,0,{}));}
 auto full=limits.result();CHECK(!limits.add(1,0,{})&&same(full,limits.result()));
 for(int i=0;i<16;++i){CHECK(limits.newline());}
 full=limits.result();CHECK(!limits.newline()&&same(full,limits.result()));
 CHECK(!Layout({0,0,0}).valid()&&!Layout({8,-2,-1}).valid());
 CHECK(argc==2);study_font::Font font;CHECK(!font.kerning(0,0,16));CHECK(font.load_trusted(argv[1]));
 std::ifstream in(argv[1],std::ios::binary);const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});
 const auto u16=[&](std::size_t p){return unsigned(bytes.at(p))*256+bytes.at(p+1);};
 const auto u32=[&](std::size_t p){return std::size_t(u16(p))*65536+u16(p+2);};
 std::size_t kern=0;for(unsigned i=0;i<u16(4);++i){auto p=12+16*i;if(bytes.at(p)=='k'&&bytes.at(p+1)=='e'&&bytes.at(p+2)=='r'&&bytes.at(p+3)=='n')kern=u32(p+8);}
 CHECK(kern&&u16(kern)==0&&u16(kern+2)==1);const auto sub=kern+4;CHECK(u16(sub+4)==1);const auto pairs=u16(sub+6);CHECK(pairs==113);
 for(unsigned i=0;i<pairs;++i){auto p=sub+14+6*i;const int raw=int(u16(p+4));const int value=raw>=32768?raw-65536:raw;
  for(float h:{16.f,32.f,64.f}){auto got=font.kerning(int(u16(p)),int(u16(p+2)),h);CHECK(got&&close(*got,value*h/1150));}}
 CHECK(!font.kerning(-1,34,16)&&!font.kerning(34,100000,16)&&!font.kerning(34,55,0));
 const auto label=study_labels::decode("AV\nA V\n");CHECK(label);auto paragraph=study_font::prepare_paragraph(font,*label,32);CHECK(paragraph);
 const float scale=32.f/1150;auto& p=*paragraph;CHECK(p.count==5&&p.layout.lines==3);
 CHECK(close(p.items[1].pen_x,(727-92)*scale));CHECK(close(p.layout.line_widths[0],(727+666-92)*scale));
 CHECK(close(p.items[2].pen_x,0)&&close(p.items[2].baseline,32));CHECK(close(p.items[4].pen_x,(727+280)*scale));
 CHECK(close(p.layout.width,(727+280+666)*scale)&&close(p.layout.height,96));
 const auto blank=study_font::prepare_paragraph(font,*study_labels::decode("\n\n"),32);CHECK(blank&&blank->count==0&&blank->layout.lines==3&&!blank->layout.ink);
 for(auto text:{"\t","\r","A\r\nV"})CHECK(!study_font::prepare_paragraph(font,*study_labels::decode(text),16));
 const auto empty=study_font::prepare_paragraph(font,{},16);CHECK(empty&&empty->layout.lines==1&&empty->layout.height==16&&empty->layout.width==0&&!empty->layout.ink);
 auto over=*label;over.count=17;CHECK(!study_font::prepare_paragraph(font,over,32));
 std::printf("Layout: %d synthetic placements, 113 kern pairs x3 sizes, bounds/failure rollback, empty/trailing lines and shared paragraph metrics passed\n",cases);
}
