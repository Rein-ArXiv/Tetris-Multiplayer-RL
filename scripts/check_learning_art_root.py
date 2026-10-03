"""Actual presentation entry points: invalid layout and clock conversion regressions."""
from pathlib import Path
import subprocess
from check_learning_utf8 import cut
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/71-character-art-check'
HEAD=r'''#include "src/image_fit.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using ImageHandle=std::uint64_t;
struct Color{unsigned char r,g,b,a;};
struct Theme{Color player{91,203,238,255},opponent{239,118,154,255};int periodMs=2400;}theme;
struct Draw{image_fit::Rect box;Color color;};
static std::vector<Draw> boxes;static std::vector<image_fit::Rect> images;
static int source_w=20,source_h=40;
static bool image_size(ImageHandle id,int& w,int& h){w=source_w;h=source_h;return id!=0;}
static void draw_rect(int x,int y,int w,int h,Color c){boxes.push_back({{x,y,w,h},c});}
static void draw_image(ImageHandle,int x,int y,int w,int h){images.push_back({x,y,w,h});}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"presentation line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
'''
def source(s):return HEAD+cut(s,'void presentation_draw_avatar(')+cut(s,'void presentation_draw_portrait(')
def binary(name,body):
 p=OUT/(name+'.cpp');p.write_text(body);b=OUT/name
 run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(b)])
 return b
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 before=OUT/'before-src-presentation.cpp'
 if before.exists():
  b=binary('presentation-before',source(before.read_text())+r'''int main(int argc,char**){presentation_draw_avatar(1,argc==1?2147483647:0,0,32,false,argc==1?0:std::numeric_limits<double>::quiet_NaN(),true);}''')
  for label,args,needle in [('overflow',[],'signed integer overflow'),('nan',['nan'],'range of representable values')]:
   r=subprocess.run([str(b),*args],capture_output=True,text=True);(OUT/f'presentation-before-{label}.log').write_text(r.stdout+r.stderr)
   assert r.returncode!=0 and needle in r.stderr,r.stderr
   print('Before presentation: '+label+' reproduced')
 after=source((ROOT/'src/presentation.cpp').read_text())+r'''
int main(){
 for(int size:{(-2147483647-1),-1,0,1,2,3}){presentation_draw_avatar(1,0,0,size,false,0,false);CHECK(boxes.empty()&&images.empty());}
 presentation_draw_avatar(1,2147483647,0,32,false,0,false);CHECK(boxes.empty()&&images.empty());
 presentation_draw_portrait(1,2147483647,0,100,100);CHECK(boxes.empty()&&images.empty());
 const double hi=(std::numeric_limits<double>::max)(),nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
 for(double time:{0.,.25,2.4,hi,-hi,nan,inf,-inf})for(bool animate:{false,true}){
  boxes.clear();images.clear();presentation_draw_avatar(1,10,20,48,false,time,animate);
  CHECK(boxes.size()==2&&images.size()==1&&boxes[0].color.a>=165);
  CHECK(images[0].x==24&&images[0].y==24&&images[0].width==20&&images[0].height==40);
  if(!animate||!std::isfinite(time))CHECK(boxes[0].color.a==255);
 }
 boxes.clear();images.clear();source_w=1000;source_h=1;
 presentation_draw_portrait(1,0,0,4,4);CHECK(images.empty());
 presentation_draw_avatar(1,0,0,12,false,0,false);CHECK(boxes.size()==2&&images.empty());
 images.clear();source_w=20;source_h=40;presentation_draw_portrait(1,0,0,100,80);
 CHECK(images.size()==1&&images[0].x==30&&images[0].y==0&&images[0].width==40&&images[0].height==80);
 images.clear();presentation_draw_portrait(0,0,0,100,80);CHECK(images.empty());
 std::puts("Root presentation: invalid/tiny/overflow layout, contain/omission, missing handles and finite/huge/nonfinite clock conversion passed");}
'''
 b=binary('presentation-after',after);r=run([str(b)]);(OUT/'presentation-after.log').write_text(r.stdout);print(r.stdout.strip())
if __name__=='__main__':main()
